import json
import urllib.parse
import boto3
from io import BytesIO
from PIL import Image, ImageDraw, ImageFont

print('Функція завантажена')

s3 = boto3.client('s3')
rekognition_client = boto3.client('rekognition')

def lambda_handler(event, context):
    print("Отримано event: " + json.dumps(event, indent=2))

    # Словник для збереження інформації про кожну виявлену цифру
    digit_info = {}

    # Отримуємо ім’я бакету та ключ з S3
    bucket = event['Records'][0]['s3']['bucket']['name']
    key = urllib.parse.unquote_plus(event['Records'][0]['s3']['object']['key'], encoding='utf-8')
    print("Ім’я файлу зображення: " + key)

    # Відкриваємо зображення з S3 як об’єкт Pillow
    s3_resource = boto3.resource('s3')
    s3_object = s3_resource.Bucket(bucket).Object(key)
    s3_response = s3_object.get()
    stream = BytesIO(s3_response['Body'].read())
    image = Image.open(stream)

    # Підготовка полотна для візуалізації результатів
    img_width, img_height = image.size
    draw = ImageDraw.Draw(image)

    try:
        # Викликаємо Rekognition для виявлення тексту
        response = rekognition_client.detect_text(Image={'S3Object': {'Bucket': bucket, 'Name': key}})
        text_detections = response['TextDetections']
        print('Виявлений текст\n----------')

        for text in text_detections:
            print('Виявлено текст: ' + text['DetectedText'])
            print('Точність: ' + "{:.2f}".format(text['Confidence']) + "%")
            print('Id: {}'.format(text['Id']))
            if 'ParentId' in text:
                print('Parent Id: {}'.format(text['ParentId']))
            print('Тип:' + text['Type'])
            print()

            # Ігноруємо текст з ParentId (альтернативні варіанти)
            # Очищення: прибираємо пробіли, замінюємо недіджит символи на 0
            if 'ParentId' not in text:
                digit = text['DetectedText'].replace(" ", "")
                digit = ''.join('0' if not char.isdigit() else char for char in digit)
                left_position = round(text['Geometry']['BoundingBox']['Left'], 2)
                digit_info[left_position] = str(digit)

            # Візуалізація рамки навколо виявленого тексту
            box = text['Geometry']['BoundingBox']
            left = img_width * box['Left']
            top = img_height * box['Top']
            width = img_width * box['Width']
            height = img_height * box['Height']
            points = (
                (left, top),
                (left + width, top),
                (left + width, top + height),
                (left, top + height),
                (left, top)
            )
            draw.line(points, fill='#00d400', width=2)
            # Малюємо сам текст на зображенні
            try:
                draw.text((left + 2, top + 2), text['DetectedText'], fill="black")
            except Exception as e:
                print("Не вдалося намалювати текст:", e)

        # Зберігаємо нове зображення з візуалізацією результатів у іншу папку
        img_bytes = BytesIO()
        image.save(img_bytes, format='JPEG')
        img_bytes = img_bytes.getvalue()
        filename = key.replace("/pic_taken", "/rekognition_result")
        s3.upload_fileobj(BytesIO(img_bytes), 'mybucket-lichilniki', filename)

        # Сортуємо цифри за їхньою позицією (зліва направо)
        digit_info = dict(sorted(digit_info.items()))
        print("Відсортований словник цифр:")
        for left, digit in digit_info.items():
            print(left, digit)

        # Об’єднуємо всі цифри в одне число
        number = ""
        for num in digit_info.values():
            number += num

        print("Отримане число: " + number)

        # Вставляємо дані в DynamoDB
        insert_data_to_db(number, key)

    except Exception as e:
        print(e)
        print(f'Помилка отримання об’єкта {key} з бакету {bucket}. Перевірте наявність об’єкта та регіон бакету.')
        raise e

def insert_data_to_db(number, key):
    # Генеруємо id запису з частини імені файлу (наприклад, images/pic_taken/31082023-143811.jpg → 31082023-143811)
    record_id = key.rsplit('/', 1)[-1].replace('.jpg', '')
    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table('meter-readings')  # Назва таблиці
    if number == "":
        number = 0  # Якщо є проблема з читанням числа, присвоюємо 0
    response = table.put_item(
        Item={
            'id': record_id,
            'numberRead': str(number),
        }
    )