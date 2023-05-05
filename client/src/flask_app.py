from flask import Flask, render_template
import os
import mqtt_decoder

# Flask application
TEMPLATE_PATH = os.path.dirname(__file__) + "/../templates"
STATIC_PATH = os.path.dirname(__file__) + "/../static"
app = Flask(__name__, template_folder=TEMPLATE_PATH, static_folder=STATIC_PATH)

# Index page
@app.route('/')
def index():  

    # Get the data
    current_data = mqtt_decoder.collect_data()
    num_points = len(current_data)

    # Load the template
    return render_template(
        'index.html', 
        num_points = num_points, 
        data = current_data,
    )

# Bus page(s)
@app.route('/buses/')
@app.route('/buses/<device_id>')
def buses(device_id=None):

    # Get data
    current_data = mqtt_decoder.collect_data()
    num_points = len(current_data)

    # Get unique devices, cast ID's to uppercase
    dev_list = set()
    for i in range(num_points):
        current_data[i]['device_id'] = current_data[i]['device_id'].upper()
        dev_list.add(current_data[i]['device_id'])
    dev_list = list(dev_list)
    dev_list == dev_list.sort()

    # Cast to requested ID to uppercase if provided
    dev_id = device_id
    if (device_id != None):
        dev_id = device_id.upper()

    # Retrieve the API key
    api_key = mqtt_decoder.get_api_key()

    # Load the template
    return render_template(
        'buses.html', 
        num_points = num_points, 
        data = current_data,
        device_id = dev_id,
        device_list = dev_list,
        num_devices = len(dev_list),
        api_key = api_key,
    )

# About page
@app.route('/about/')
def about():  

    # Load static template
    return render_template('about.html')

# Handle errors
@app.errorhandler(Exception)
def error_handle(error):
    error_code = str(error).split()[0]
    if (error_code == 'list'):
        error_code = 404
    return render_template("error.html", error_code=error_code)
