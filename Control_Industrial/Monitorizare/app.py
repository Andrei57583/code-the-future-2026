from flask import Flask, render_template
from flask_socketio import SocketIO, emit
from paho.mqtt import client as mqtt_client
import json

app = Flask(__name__)

socketio = SocketIO(app, cors_allowed_origins='*')

MQTT_BROKER = "127.0.0.1"
MQTT_TOPIC = "esp32/data"

def on_message(client, userdata, msg):
    data_ditc = {}
    try:
        raw_payload = msg.payload

        if isinstance(raw_payload,dict):
            data_ditc = raw_payload
        else:
            decoded_payload = raw_payload.decode().strip()
            decoded_payload = decoded_payload.replace("nan", "null")
            data_ditc = json.loads(decoded_payload)
            print(f"{data_ditc}")

        socketio.emit("mqtt_update", {'data': data_ditc})
        print("Date trimise cu succes")
    except json.JSONDecodeError:
        print(f"Eroare: Mesajul primit nu este un json valid - {msg.payload}")
    except Exception as e:
        print(f"Eroare la procesarea datelor: {e}")

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