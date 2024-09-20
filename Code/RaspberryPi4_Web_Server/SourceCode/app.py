from flask import Flask, render_template, jsonify
import mysql.connector
from mysql.connector import Error
import RPi.GPIO as GPIO
import board
import adafruit_dht
import time
from multiprocessing import Process

app = Flask(__name__)

GPIO.setmode(GPIO.BCM)
DEVICE1_PIN = 20
DEVICE2_PIN = 21
GPIO.setup(DEVICE1_PIN, GPIO.OUT)
GPIO.setup(DEVICE2_PIN, GPIO.OUT)

DHT_PIN = board.D27
dht_device = adafruit_dht.DHT11(DHT_PIN)

db_config = {
    'host': 'localhost',
    'user': 'home_user',
    'password': 'mypassword',
    'database': 'home_data'
}

device1_status = False
device2_status = False

def read_and_store_sensor_data():
    while True:
        try:
            temperature = dht_device.temperature
            humidity = dht_device.humidity

            if temperature is not None and humidity is not None:
                try:
                    connection = mysql.connector.connect(**db_config)
                    if connection.is_connected():
                        cursor = connection.cursor()
                        sql_insert_query = """
                        INSERT INTO readings (temperature, humidity, device_status, device2_status)
                        VALUES (%s, %s, %s, %s)
                        """
                        cursor.execute(sql_insert_query, (temperature, humidity, device1_status, device2_status))
                        connection.commit()
                        cursor.close()
                        connection.close()
                except Error as e:
                    pass

        except Exception as e:
            pass

        time.sleep(30)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/update_device/<device>/<action>')
def update_device(device, action):
    global device1_status, device2_status

    if device == 'device1':
        if action == 'toggle':
            device1_status = not device1_status
        status = device1_status
    elif device == 'device2':
        if action == 'toggle':
            device2_status = not device2_status
        status = device2_status
    else:
        return jsonify({'error': 'Invalid device'}), 400

    GPIO.output(DEVICE1_PIN, GPIO.HIGH if device1_status else GPIO.LOW)
    GPIO.output(DEVICE2_PIN, GPIO.HIGH if device2_status else GPIO.LOW)

    try:
        connection = mysql.connector.connect(**db_config)
        if connection.is_connected():
            cursor = connection.cursor()
            if device == 'device1':
                sql_update_query = """
                UPDATE readings
                SET device_status = %s
                WHERE id = (SELECT id FROM readings ORDER BY timestamp DESC LIMIT 1)
                """
                cursor.execute(sql_update_query, (device1_status,))
            elif device == 'device2':
                sql_update_query = """
                UPDATE readings
                SET device2_status = %s
                WHERE id = (SELECT id FROM readings ORDER BY timestamp DESC LIMIT 1)
                """
                cursor.execute(sql_update_query, (device2_status,))
            connection.commit()
            cursor.close()
            connection.close()
    except Error as e:
        return jsonify({'error': 'Database error'}), 500

    return jsonify({
        'device': device,
        'device1_status': device1_status,
        'device2_status': device2_status
    })

@app.route('/data')
def data():
    connection = None
    data = {'temperature': [], 'humidity': [], 'timestamp': []}
    try:
        connection = mysql.connector.connect(**db_config)
        if connection.is_connected():
            cursor = connection.cursor(dictionary=True)
            query = "SELECT temperature, humidity, timestamp FROM readings ORDER BY timestamp DESC LIMIT 7"
            cursor.execute(query)
            results = cursor.fetchall()
            for row in results:
                data['temperature'].append(row['temperature'])
                data['humidity'].append(row['humidity'])
                data['timestamp'].append(row['timestamp'].isoformat())
    except Error as e:
        pass
    finally:
        if connection and connection.is_connected():
            cursor.close()
            connection.close()
    return jsonify(data)

if __name__ == '__main__':
    sensor_process = Process(target=read_and_store_sensor_data)
    sensor_process.start()

    app.run(host='0.0.0.0', port=80, debug=True)

    sensor_process.join()
