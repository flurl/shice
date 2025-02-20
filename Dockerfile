# pull official base image
FROM python:3.11-slim-bookworm
# set work directory
WORKDIR /app/

# set environment variables
ENV PYTHONUNBUFFERED 1

# install system dependencies
#RUN apt-get update && apt-get install -y imagemagick --no-install-recommends

# install dependencies
RUN pip install --upgrade pip
COPY ./requirements.txt .
RUN pip install -r requirements.txt

# copy entrypoint.sh
#COPY ./entrypoint.sh .
#RUN sed -i 's/\r$//g' /usr/src/epaper_converter/entrypoint.sh
#RUN chmod +x /usr/src/epaper_converter/entrypoint.sh

# copy project
COPY . .

# run entrypoint.sh
#ENTRYPOINT ["/usr/src/epaper_converter/entrypoint.sh"]
CMD ["python", "manage.py"]
