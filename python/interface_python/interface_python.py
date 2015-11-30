""" 1st multiprocess tests with python"""

import logging
import time
import signal
import multiprocessing
from multiprocessing.sharedctypes import Value
from struct import *
import json
from twisted.internet.defer import inlineCallbacks
from autobahn.twisted.util import sleep
from autobahn.twisted.wamp import ApplicationSession
from twisted.internet import reactor
import sys
import serial


class Component(ApplicationSession):

    """
    An application component that publishes data coming from
    arduino and send any order to
    arduino via two queues shared between both processes
    """
    #
    # more usefull variables
    #

    """ Visual reference:
    "command"  - 0,
    "ForceCil" - 1,
    "ForceCal" - 2,
    "Hour2" - 3,
    "Hour1" - 4,
    "BombCil" - 5,
    "BombCal" - 6,
    "year" - 7,
    "month" - 8,
    "day" - 9,
    "hour" - 10,
    "minute" - 11,
    "sec" - 12,
    "threashold" - 13,
    "aHour1_start" - 14,
    "aMin1_start" - 15,
    "aHour1_end" - 16,
    "aMin1_end" - 17,
    "aHour2_start" - 18,
    "aMin2_start" - 19,
    "aHour2_end" - 20,
    "aMin2_end" - 21,
    "TempCal" - 22,
    "TempCil" - 23,
    "TempAmb" - 24,
    "TempMin" - 25
    """
    values = [0] * 26

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

    def getDateTime(self, values):
        tm = time.localtime()
        values[7] = tm.tm_year
        values[8] = tm.tm_mon
        values[9] = tm.tm_mday
        values[10] = tm.tm_hour
        values[11] = tm.tm_min
        values[12] = tm.tm_sec
        return values

    ######################################################################
    #   def of functions used to send orders to arduino
    ######################################################################
    def sync_date(self, order, values):
        values[0] = 'T'
        values = self.getDateTime(values)

    def activate_motorCal(self, order, values):
        values[0] = 'f'
        values = self.getDateTime(values)
        if(order['state'] == 1):
            values[2] = 1
        else:
            values[2] = 0

    def activate_motorCil(self, order, values):
        values[0] = 'F'
        values = self.getDateTime(values)
        if(order['state'] == 1):
            values[1] = 1
        else:
            values[1] = 0

    def activate_hour1(self, order, values):
        values[0] = 'h'
        values = self.getDateTime(values)
        if (order['state'] == 0):
            values[4] = 0
        else:
            values[4] = 1
            values[14] = order['start'][0]
            values[15] = order['start'][1]
            values[16] = order['end'][0]
            values[17] = order['end'][1]

    def activate_hour2(self, order, values):
        values[0] = 'H'
        values = self.getDateTime(values)
        if (order['state'] == 0):
            values[3] = 0
        else:
            values[3] = 1
            values[18] = order['start'][0]
            values[19] = order['start'][1]
            values[20] = order['end'][0]
            values[21] = order['end'][1]

    def temp_min_Cil(self, order, values):
        values[0] = 't'
        values = self.getDateTime(values)
        values[25] = float(order['value'])

    def temp_thresh_Cil(self, order, values):
        values[0] = 'o'
        values = self.getDateTime(values)
        values[13] = order['value']
    #######################################################################
    """ No switch cases in python?!?!?!?
    functions dictionary defined to replace typical switch cases
    """
    funcdict = {'sync_date': sync_date,
                'activate_motorCal': activate_motorCal,
                'activate_motorCil': activate_motorCil,
                'activate_hour1': activate_hour1,
                'activate_hour2': activate_hour2,
                'temp_min_Cil': temp_min_Cil,
                'temp_thresh_Cil': temp_thresh_Cil
                }

    #######################################################################
    # Autobahn main components to get events. See templates to understand
    # simple exemples
    #######################################################################
    @inlineCallbacks
    def onJoin(self, details):
        print("session attached")
        # SUBSCRIBE to a topic and receive events
        #

        def process_order(msg):
            # print "got this order", msg
            command = json.loads(msg)
            command = self._decode_dict(command)
            # print command
            myfunc = self.funcdict[command["command"]]
            # print myfunc
            myfunc(self, command, self.values)
            orders.put(self.values)
            # print self.values
            # reset values to all zeros after processing
            self.values = [0] * 26

        sub = yield self.subscribe(process_order, 'com.arduino.order')
        print("subscribed to topic 'com.arduino.order': {}".format(sub))

        #
        # PUBLISH an event
        #
        while True:
            if(exitFlag.value):
                reactor.stop()
                return
            if not queue.empty():
                I = queue.get()
                yield self.publish('com.arduino.state', I)
            # else:
            #    print ("No items in queue")
            yield sleep(0.1)


###############################################################################
# Publisher/subscriber process code with uses the class defined above
###############################################################################
def pubsub_manager(queue, orders):
    from autobahn.twisted.wamp import ApplicationRunner
    runner = ApplicationRunner(url=u"ws://192.168.1.73:8080/ws", realm=u"realm1")
    runner.run(Component)


###############################################################################
# Arduino process code
###############################################################################
def reading_arduino(port, queue, orders):
    """
    this section defines the code for a new process to change information with
    arduino. Information coming from arduino are sent to publisher via queue
    "queue" Orders to arduino from subscriber are sent via queue "orders"
    """
    keys = (
        "command", "ForceCil", "ForceCal", "Hour2", "Hour1",
        "BombCil", "BombCal", "year", "month", "day", "hour", "minute", "sec",
        "threashold", "aHour1_start", "aMin1_start", "aHour1_end", "aMin1_end",
        "aHour2_start", "aMin2_start", "aHour2_end", "aMin2_end", "TempCal",
        "TempCil", "TempAmb", "TempMin"
        )
    values = [0] * 26
    curr_state = dict(zip(keys, values))
    myJson = ""
    print ("start of reading arduino")
    while True:
        # ordered to exit?
        if(exitFlag.value):
            return
        # any command to send to arduino?
        if not orders.empty():
            obj = orders.get()
            new_order = pack('<c6BH14B4f', *obj)
            # print new_order
            num_bytes = port.write(new_order)
            port.flush()  # wait for data to be writen
            if (num_bytes != 39):
                print("Something was wrong!!!")

        # wait for data from arduino
        myByte = port.read(1)
        if myByte == 'S':
            data = port.read(39)
            myByte = port.read(1)
            if myByte == 'E':
                # is  a valid message struct
                new_values = unpack('<c6BH14B4f', data)
                # print new_values
                curr_state = dict(zip(keys, new_values))
                # print curr_state
                myJson = json.dumps(curr_state)
                # send to publisher queue for dispatch
                queue.put(myJson)
        else:
            sys.stdout.write(myByte)


if __name__ == '__main__':
    # Port to arduino
    MYPORT = serial.Serial("/dev/ttyUSB0", 57600)
    # shared variable to order exit of both processes
    exitFlag = Value('i', 0)
    # list to store processes info
    jobs = []
    multiprocessing.log_to_stderr(logging.DEBUG)
    # Definition of queues. queue is used to store data from arduino to the
    # publisher/subscriber orders is used to pass orders from
    # publisher/subscriber to arduino
    queue = multiprocessing.Queue()
    orders = multiprocessing.Queue()

    def exit_gracefully(*args):
        print("Got a SIGTERM")
        exitFlag.value = 1
        return

    try:
        print("main start")
        p = multiprocessing.Process(name="arduino", target=reading_arduino,
                                    args=(MYPORT, queue, orders))
        p.start()
        jobs.append(p)
        p = multiprocessing.Process(name="PubSub", target=pubsub_manager,
                                    args=(queue, orders))
        p.start()
        jobs.append(p)
        # by now to exit gracefully just send any key
        """ comment here!!!!
        raw_input("Press any key to end")
        exitFlag.value = 1
        """
        signal.signal(signal.SIGTERM, exit_gracefully)
    finally:
        # wait for processes to end their tasks
        print("waiting for signal")
        for item in jobs:
            item.join()
    # close queues used to interchange data
    queue.close()
    queue.join_thread()
    orders.close()
    orders.join_thread()
    # close arduino port
    MYPORT.close()
    print("exiting now: interface_python")
