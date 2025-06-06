// Імпортуємо клієнти AWS SDK
import { S3Client, PutObjectCommand } from '@aws-sdk/client-s3';
import sharp from 'sharp';

// Ініціалізуємо S3 клієнт для регіону Європа (Центральна)
const s3 = new S3Client({ region: 'eu-central-1' });

// Головна Lambda-функція
export const handler = async (event) => {
    try {
        // Зчитуємо параметри з event
        const rotateAngle = parseInt(event.rotateDegrees);
        const cropX = parseInt(event.cropLeft);
        const cropY = parseInt(event.cropTop);
        const cropWidth = parseInt(event.cropWidth);
        const cropHeight = parseInt(event.cropHeight);
        const fileName = generateTimestampedName();
        const encodedImage = event.imageData;

        console.log(`Параметри обрізки: x=${cropX}, y=${cropY}, ширина=${cropWidth}, висота=${cropHeight}, поворот=${rotateAngle}°`);

        // Декодуємо base64-рядок у Buffer
        const imageBuffer = Buffer.from(encodedImage, 'base64');

        // Обробляємо зображення: поворот + обрізка
        const processedImage = await processImage(imageBuffer, rotateAngle, cropX, cropY, cropWidth, cropHeight);

        const s3Key = `images/pic_taken/${fileName}.jpg`;
        const uploadParams = {
            Bucket: 'mybucket-lichilniki',
            Key: s3Key,
            Body: processedImage,
        };

        // Завантажуємо файл у S3
        const uploadResult = await s3.send(new PutObjectCommand(uploadParams));
        console.log('Файл успішно завантажено в S3:', uploadResult);

        return {
            statusCode: 200,
            body: JSON.stringify(uploadResult),
        };
    } catch (err) {
        console.error('Виникла помилка під час обробки зображення:', err);
        return {
            statusCode: 500,
            body: JSON.stringify({ message: 'Помилка при обробці зображення', error: err }),
        };
    }
};

// Функція для обробки зображення: поворот і обрізка
const processImage = async (buffer, angle, x, y, width, height) => {
    const { data } = await sharp(buffer)
        .rotate(angle)
        .extract({ width: width, height: height, left: x, top: y })
        .toBuffer({ resolveWithObject: true });
    return data;
};

// Функція для генерації імені файлу на основі часу (Kyiv)
const generateTimestampedName = () => {
    const kyivTime = new Date().toLocaleString('uk-UA', { timeZone: 'Europe/Kyiv' });
    const [datePart, timePart] = kyivTime.split(' ');
    const dateMatch = datePart.match(/(\d{1,2})\.(\d{1,2})\.(\d{4})/);
    const formattedDate = `${dateMatch[1]}${dateMatch[2]}${dateMatch[3]}`;
    const formattedTime = timePart.replace(/:/g, '');
    return `${formattedDate}-${formattedTime}`;
};