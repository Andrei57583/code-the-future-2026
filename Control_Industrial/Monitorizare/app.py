from flask import Flask, render_template
from flask_socketio import SocketIO, emit
from paho.mqtt import client as mqtt_client

app = Flask(__name__)

socketio = SocketIO(app, cors_allowed_origins='*')

MQTT_BROKER = "127.0.0.1"
MQTT_TOPIC = "esp32/data"

def on_message(client, userdata, msg):
    data = msg.payload.decode()
    print(f"MesaJ: {data}")
    socketio.emit("mqtt_update", {'valoare': data}, namespace="/")

mqttClient = mqtt_client.Client()
mqttClient.on_message = on_message
mqttClient.connect(MQTT_BROKER, 1883)
mqttClient.subscribe(MQTT_TOPIC)
mqttClient.loop_start()

@app.route('/')

def home():
    return render_template('index.html')

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000,debug=True)