import os
import argparse
import pathlib
import shutil
import json
import numpy as np
import tflite_runtime.interpreter as tflite
import PIL.Image

import settings
import requests

class Model:
    def __init__(self, model_filepath):
        self.interpreter = tflite.Interpreter(model_path=str(model_filepath))
        self.interpreter.allocate_tensors()

        self.input_details = self.interpreter.get_input_details()
        self.output_details = self.interpreter.get_output_details()
        assert len(self.input_details) == 1 and len(self.output_details) == 1
        self.input_shape = self.input_details[0]['shape'][1:3]

        self.labels = self.read_labels_from_file(pathlib.Path(model_filepath).resolve().parent / "labels.txt")



    def read_labels_from_file(self, file):
        with open(file) as f:
            return f.read().splitlines()


    def predict(self, image_filepath):
        image = PIL.Image.open(image_filepath).resize(self.input_shape)
        input_array = np.array(image, dtype=np.float32)[np.newaxis, :, :, :]

        self.interpreter.set_tensor(self.input_details[0]['index'], input_array)
        self.interpreter.invoke()

        predictions = {}
        for detail in self.output_details:
            predictions[detail['name']] = {}
            for i, score in enumerate(self.interpreter.get_tensor(detail['index'])[0]):
                predictions[detail['name']][self.labels[i]] = float(score) 
        print(predictions)
        return predictions


def print_outputs(outputs):
    outputs = list(outputs.values())[0]
    for index, score in enumerate(outputs[0]):
        print(f"Label: {index}, score: {score:.5f}")


#def print_outputs(outputs):
#    outputs = list(outputs.values())[0]
#    for index, score in enumerate(outputs[0]):
#        print(f"Label: {index}, score: {score:.5f}")


def save_results(file, results):
    if not os.path.exists(settings.TAGGED_PATH):
        os.makedirs(settings.TAGGED_PATH)
    shutil.move(file, settings.TAGGED_PATH)

    p = pathlib.Path(file)
    print(str(results))
    with open(pathlib.Path(settings.TAGGED_PATH) / f"{p.stem}.json", "w") as jfile:
        json.dump(results, jfile)



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('model_filepath', type=pathlib.Path)
    parser.add_argument('image_filepath', type=pathlib.Path)

    args = parser.parse_args()

    model = Model(args.model_filepath)
    outputs = model.predict(args.image_filepath)
    save_results(args.image_filepath, outputs['model_output'])
    label = max(outputs['model_output'], key=outputs['model_output'].get)
    requests.post(settings.POST_PREDICTION_HOOK, data={"label": label}, verify=False)
    #print_outputs(outputs)


if __name__ == '__main__':
    main()
