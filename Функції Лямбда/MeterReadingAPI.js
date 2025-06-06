import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient, GetCommand, PutCommand, ScanCommand, DeleteCommand } from "@aws-sdk/lib-dynamodb";

// Назва таблиці DynamoDB
const tableName = "meter-readings";

// Шляхи для API Gateway
const singleDataPath = "/electricitymeter/data";
const allDataPath = "/electricitymeter/alldata";

// Ініціалізуємо клієнт DynamoDB
const client = new DynamoDBClient({ region: "eu-central-1" });
const dynamodb = DynamoDBDocumentClient.from(client);

// Основна Lambda-функція
export const handler = async (event) => {
    console.log("Отримано запит:", JSON.stringify(event));

    let response;

    try {
        // Вибираємо дію залежно від httpMethod і path
        switch (true) {
            case event.httpMethod === 'GET' && event.path === singleDataPath:
                response = await getSingleData(event.queryStringParameters.id);
                break;
            case event.httpMethod === 'GET' && event.path === allDataPath:
                response = await getAllData();
                break;
            case event.httpMethod === 'POST' && event.path === singleDataPath:
                response = await saveData(JSON.parse(event.body));
                break;
            case event.httpMethod === 'DELETE' && event.path === singleDataPath:
                response = await deleteData(JSON.parse(event.body).id);
                break;
            default:
                // Якщо маршрут не знайдено
                response = buildResponse(404, { message: "404 Not Found" });
        }
    } catch (error) {
        console.error("Помилка під час обробки запиту:", error);
        response = buildResponse(500, { message: "Internal Server Error", error: error.message });
    }

    return response;
};

// Отримати окремий запис з DynamoDB
async function getSingleData(dataId) {
    try {
        const params = {
            TableName: tableName,
            Key: { id: dataId }
        };
        const result = await dynamodb.send(new GetCommand(params));
        return buildResponse(200, result.Item || { message: "Дані не знайдено" });
    } catch (error) {
        console.error("Помилка отримання окремих даних:", error);
        return buildResponse(500, { message: "Internal Server Error", error: error.message });
    }
}

// Функція для перетворення ID в порівнянний рядок дати YYYYMMDDHHMMSS
function parseIdForSorting(id) {
    if (typeof id !== 'string' || !/^\d{8}-\d{6}$/.test(id)) {
        // Повертаємо значення, яке відправить невалідні ID в кінець списку при сортуванні за спаданням
        return "00000000-000000"; // або null, або інше значення для обробки
    }
    const parts = id.split('-');
    const datePart = parts[0]; // DDMMYYYY
    const timePart = parts[1]; // HHMMSS
    const day = datePart.substring(0, 2);
    const month = datePart.substring(2, 4);
    const year = datePart.substring(4, 8);
    return `${year}${month}${day}${timePart}`; // YYYYMMDDHHMMSS
}

// Отримати всі записи з DynamoDB та відсортувати їх
async function getAllData() {
    try {
        const params = {
            TableName: tableName
        };
        const result = await dynamodb.send(new ScanCommand(params));
        let items = result.Items || [];

        // Сортування даних: новіші спочатку
        items.sort((a, b) => {
            const dateA = parseIdForSorting(a.id);
            const dateB = parseIdForSorting(b.id);

            // Порівнюємо як рядки; YYYYMMDDHHMMSS формат дозволяє це
            // Для сортування за спаданням (новіші спочатку)
            if (dateA > dateB) return -1;
            if (dateA < dateB) return 1;
            return 0;
        });

        return buildResponse(200, { data: items });
    } catch (error) {
        console.error("Помилка отримання всіх даних:", error);
        return buildResponse(500, { message: "Internal Server Error", error: error.message });
    }
}

// Зберегти (створити або оновити) запис у DynamoDB
async function saveData(requestBody) {
    try {
        const params = {
            TableName: tableName,
            Item: requestBody
        };
        await dynamodb.send(new PutCommand(params));
        return buildResponse(200, {
            Operation: "SAVE",
            Message: "SUCCESS",
            Item: requestBody
        });
    } catch (error) {
        console.error("Помилка збереження даних:", error);
        return buildResponse(500, { message: "Internal Server Error", error: error.message });
    }
}

// Видалити запис з DynamoDB
async function deleteData(id) {
    try {
        const params = {
            TableName: tableName,
            Key: { id: id }
        };
        const result = await dynamodb.send(new DeleteCommand(params));
        return buildResponse(200, {
            Operation: "DELETE",
            Message: "SUCCESS",
            Item: result.Attributes || { message: "Дані не знайдено" }
        });
    } catch (error) {
        console.error("Помилка видалення даних:", error);
        return buildResponse(500, { message: "Internal Server Error", error: error.message });
    }
}

// Формуємо стандартну HTTP-відповідь
function buildResponse(statusCode, body) {
    return {
        statusCode,
        headers: {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*" // Додано для CORS, якщо потрібно
        },
        body: JSON.stringify(body)
    };
}