import sqlite3
import json
from twisted.internet.defer import inlineCallbacks
from autobahn.twisted.util import sleep
from autobahn.twisted.wamp import ApplicationSession
import datetime

table_name = "data"
"""
table data (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp DATETIME,
            TempCal REAL, TempCil REAL)
"""
# Creates or opens a file called tempdb with a SQLite3 DB


class sqlPlotter(ApplicationSession):
    conn = ""
    c = ""
    last_commit = ""
    first_commit = 0
    #######################################################################
    # This block section was taken to remove the unicode from json.loads
    #######################################################################

    def _decode_list(self, data):
        rv = []
        for item in data:
            if isinstance(item, unicode):
                item = item.encode('utf-8')
            elif isinstance(item, list):
                item = self._decode_list(item)
            elif isinstance(item, dict):
                item = self._decode_dict(item)
            rv.append(item)
        return rv

    def _decode_dict(self, data):
        rv = {}
        for key, value in data.iteritems():
            if isinstance(key, unicode):
                key = key.encode('utf-8')
            if isinstance(value, unicode):
                value = value.encode('utf-8')
            elif isinstance(value, list):
                value = self._decode_list(value)
            elif isinstance(value, dict):
                value = self._decode_dict(value)
            rv[key] = value
        return rv
    ######################################################################
    ######################################################################

    def connect_sql(self):
        self.conn = sqlite3.connect('templog.db')
        self.c = self.conn.cursor()

    def insert_data(self, data):
        self.c.execute('''INSERT INTO data (timestamp, TempCal, TempCil) VALUES(datetime('now'),?,?)''',
                       (data[0], data[1]))

    def close_sql(self):
        self.conn.commit()
        self.conn.close()

    def getSql(self):
        self.c.execute('''SELECT * FROM data WHERE timestamp BETWEEN datetime('now', '-6 hours') AND datetime('now')''')
        info = self.c.fetchall()
        return info

    @inlineCallbacks
    def onJoin(self, details):
        print("session attached")

        def get_temps(msg):
            data = []
            info = json.loads(msg)
            info = self._decode_dict(info)
            if (self.first_commit == 0):
                self.last_commit = datetime.datetime.now()
                data.append(info['TempCal'])
                data.append(info['TempCil'])
                self.connect_sql()
                self.insert_data(data)
                # publish last 24h values
                self.close_sql()
                self.first_commit = 1
            else:
                curr_time = datetime.datetime.now()
                diff_time = curr_time-self.last_commit
                if(diff_time.seconds >= 5*60):
                    data.append(info['TempCal'])
                    data.append(info['TempCil'])
                    self.connect_sql()
                    self.insert_data(data)
                    self.close_sql()
                    self.last_commit = curr_time

        def getSqlRequest():
            self.connect_sql()
            data = self.getSql()
            self.close_sql()
            return data

        # SUBSCRIBE to a topic and receive events
        #
        sub = yield self.subscribe(get_temps, 'com.arduino.state')
        print("subscribed to topic 'com.arduino.state': {}".format(sub))
        try:
            yield self.register(getSqlRequest, 'com.chart.sql')
        except Exception as e:
            print("failed to register procedure getSqlRequest: {}".format(e))
        else:
            print("procedure getSqlRequest registered")

if __name__ == '__main__':
    from autobahn.twisted.wamp import ApplicationRunner
    runner = ApplicationRunner(url=u"ws://localhost:8080/ws", realm=u"realm1")
    runner.run(sqlPlotter)
