import json
import base64
import random
import sys

from paho.mqtt import client as mqtt_client

# to install paho-mqtt client: pip install paho-mqtt
# or see: https://pypi.org/project/paho-mqtt/
# to receive date from Metropolia LoRaWAN you need to connect to
# Wifi: KME662
# Password: SmartIot
# run on your PC:  python lorareceive.py device_name
# for example: python lorareceive.py Overdosemachine

# the following can be given as command line parameters
default_broker = '192.168.1.10' # 2nd parameter
default_port = 1883  # 3rd parameter
default_sub_topic = "application/#"  # 4th parameter



def find(json_data, name):
    try:
        list = json.loads(json_data)
        if list["deviceInfo"]["deviceName"] == name:
            if "data" in list:
                data = base64.b64decode(list["data"])
                try:
                    text = data.decode()
                    if text.isprintable():  
                        print("Text:", data.decode())
                    else:
                        print("Binary:", data.hex().upper())
                except:
                    print("Binary:", data.hex().upper())
        #print(json.dumps(list, indent=4))
    except:
        print("Error while parsing JSON")

        
   
class LoRaReceiver:
    def __init__(self, broker, port, sub_topic, device_name):

        # generate client ID with pub prefix randomly
        self.client_id = f'{device_name}-{random.randint(0, 1000)}'
        self.device_name = device_name
        # username = 'myusername'
        # password = 'mypassword'
        print("Connecting to", broker, "with id", self.client_id)

        self.client = mqtt_client.Client(self.client_id)

        #client.username_pw_set(username, password)
        self.client.on_connect = self.on_connect
        self.client.connect(broker, port)

        self.client.on_message = self.on_message
        self.client.subscribe(sub_topic)

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    def on_message(self, client, userdata, msg):
        rd = msg.payload.decode()
        find(rd, self.device_name)
        
    def run(self):
        self.client.loop_forever()
       


def run(argv):
    if len(argv)> 0 : 
        device_name = argv[0]
    else:
        raise Exception("You must specify device name")

    if len(argv)> 1 : 
        broker = argv[1]
    else:
        broker = default_broker

    if len(argv)> 2 : 
        port = int(argv[2])
    else:
        port = default_port

    if len(argv)> 3 : 
        sub_topic = argv[3]
    else:
        sub_topic = default_sub_topic
        
    lora = LoRaReceiver(broker, port, sub_topic, device_name)
    lora.run()


if __name__ == '__main__':
    run(sys.argv[1:])
