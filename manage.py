import os
import sys
import subprocess
import datetime
import glob
import pathlib
import json
import shutil
from flask import (
    Flask, 
    render_template,
    send_from_directory,
    request,
    redirect,
    jsonify,
)

from werkzeug.utils import secure_filename

import settings

# Number of days after which files will be deleted
FILE_AGE_THRESHOLD_DAYS = 14

app = Flask(__name__)


def do_prediction(input_file):
    subprocess.Popen([sys.executable, 'predict.py', 'tensorflow/model.tflite', input_file])


def get_labels():
    with open("tensorflow/labels.txt") as f:
        return f.read().splitlines()


@app.route('/')
def root():
    return static_file('test.html', root='.')


@app.post('/upload')
def do_upload():
#    category = request.forms.get('category')
    upload = request.files['imageFile']
    name, ext = os.path.splitext(upload.filename)
    if ext not in ('.png', '.jpg', '.jpeg'):
        return "File extension not allowed."

    save_path = settings.CAPTURED_IMG_PATH
    name = "{0}{1}".format(datetime.datetime.now().strftime("%Y%m%d_%H%M%S"), ext)
    if not os.path.exists(save_path):
        os.makedirs(save_path)
        
    # Delete files older than FILE_AGE_THRESHOLD_DAYS
    current_time = datetime.datetime.now()
    for filename in os.listdir(save_path):
        file_path = os.path.join(save_path, filename)
        
        # Skip if it's a directory
        if os.path.isdir(file_path):
            continue
        
        # Get file's last modification time
        file_mod_time = datetime.fromtimestamp(os.path.getmtime(file_path))
        
        # Delete if file is older than threshold
        if (current_time - file_mod_time).days >= FILE_AGE_THRESHOLD_DAYS:
            os.remove(file_path)


    file_path = os.path.join(save_path, secure_filename(name))
    upload.save(file_path)
    do_prediction(file_path)
    return "File successfully saved to '{0}'.".format(save_path)


@app.route("/list_tagged")
def list_tagged():
    files = glob.glob(f"{settings.TAGGED_PATH}/*.jpg")
    files.sort(reverse=True) 
    images = []
    for filename in files:
        with open(pathlib.Path(filename).with_suffix(".json")) as scores_file:
            scores = json.load(scores_file)
            tag = max(scores, key=scores.get)
            images.append({"src": filename, "scores": scores, "tag": tag})
    context = {
                'app_name': settings.APP_NAME,
                'images': images,
                'labels': get_labels(),
              }
    
    return render_template("list_tagged.html", **context)


# this route is used to serve the images from /tagged
@app.route("/tagged/<path:path>")
def serve_tagged(path):
    return send_from_directory(settings.TAGGED_PATH, path)


# manually tag an image
@app.route("/tag", methods=["GET", "POST"])
def tag():
    if request.method == 'GET':
        files = [pathlib.Path(request.args.get("file"))]
    elif request.method == 'POST':
        files = request.form.getlist("file")
    
    label = request.args.get("label")

    for file in files:
        json_file = pathlib.Path(file).with_suffix(".json")

        with open(json_file) as f:
            scores = json.load(f)

        for t in scores:
            scores[t] = 0.0

        scores[label] = 1.0

        with open(json_file, "w") as f:
            json.dump(scores, f)

    return redirect("/list_tagged")


@app.route("/delete", methods=["GET", "POST"])
def delete():
    if request.method == 'GET':                           
        files = [pathlib.Path(request.args.get("file"))]  
    elif request.method == 'POST':                        
        files = request.form.getlist("file")              

    for file in files:
        file = pathlib.Path(file)
        os.remove(file)
        json_file = file.with_suffix(".json")
        os.remove(json_file)
    if request.accept_mimetypes.accept_json:
        return ('', 204)
    else:
        return redirect("/list_tagged")


@app.route("/prepare_training_data")
def prepare_training_data():
    f = request.args.get("file")
    if f is None:
        files = glob.glob(f"{settings.TAGGED_PATH}/*.jpg")
    else:
        files = [f]
    for file in files:
        p = pathlib.Path(file)
        score_file = p.with_suffix(".json")
        with open(score_file) as f:
            scores = json.load(f)
        label = max(scores, key=scores.get)
        label_path = f"{settings.TRAINING_DATA_PATH}{label}"
        if not os.path.exists(label_path):
            os.makedirs(label_path)

        shutil.move(file, label_path)
        os.remove(score_file)

    if request.accept_mimetypes.accept_json:
        return ('', 204)
    else:
        return redirect("/list_tagged")


if __name__ == '__main__':
    from werkzeug.debug import DebuggedApplication
    from werkzeug import run_simple
    os.environ['WERKZEUG_DEBUG_ADDR'] = '0.0.0.0'  # Allow all IP addresses
#    app.wsgi_app = DebuggedApplication(app.wsgi_app, evalex=True)
    app.trusted_hosts=["10.0.0.254"]
    app.run(host="0.0.0.0", debug=True)


