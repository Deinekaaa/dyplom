#ifndef MY_HTML_H 
#define MY_HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM Лічильник</title>
  <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
  <link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.6.0/cropper.min.css" />
  <style>
    :root {
      --primary-color: #3f51b5;
      --primary-dark: #2c3e9f;
      --start-color: #4CAF50;
      --start-dark: #388E3C;
      --stop-color: #f44336;
      --stop-dark: #d32f2f;
      --light-gray: #f0f2f5;
      --dark-gray: #333;
      --border-color: #dcdfe6;
      --shadow-color: rgba(0,0,0,0.1);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Roboto', sans-serif;
      background: var(--light-gray);
      color: var(--dark-gray);
      display: flex;
      flex-direction: column;
      min-height: 100vh;
    }
    header {
      background: var(--primary-color);
      color: #fff;
      padding: 1rem 2rem;
      text-align: center;
      box-shadow: 0 2px 8px var(--shadow-color);
      position: relative;
    }
    header h1 { font-weight: 500; font-size: 1.5rem; }
    header img {
      position: absolute;
      right: 1rem;
      top: 50%;
      transform: translateY(-50%);
      height: 50px;
    }
    main {
      flex: 1;
      padding: 1rem;
      max-width: 900px;
      margin: 0 auto;
      width: 100%;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: .5rem;
      background: var(--primary-color);
      color: #fff;
      border: none;
      padding: .75rem 1.5rem;
      border-radius: 10px;
      font-size: 1rem;
      cursor: pointer;
      transition: background .3s, transform .1s;
      text-decoration: none;
    }
    .btn:hover:not(:disabled) {
      background: var(--primary-dark);
      transform: translateY(-2px);
      box-shadow: 0 4px 12px var(--shadow-color);
    }
    .btn:disabled {
      background: #9E9E9E;
      cursor: not-allowed;
    }
    .grid { display: grid; gap: 1rem; }
    .grid-2 { grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); }
    .card {
      background: #fff;
      padding: 1.5rem;
      border-radius: 16px;
      box-shadow: 0 4px 12px var(--shadow-color);
      margin-bottom: 2rem;
      animation: fadeIn 0.5s ease;
    }
    @keyframes fadeIn { from {opacity: 0;} to {opacity: 1;} }
    #photo {
      width: 100%;
      max-height: 450px;
      border: 2px solid var(--border-color);
      border-radius: 12px;
      object-fit: contain;
      background: #fafafa;
    }
    .controls {
      display: flex;
      flex-wrap: wrap;
      gap: 1rem;
      margin-top: 1rem;
    }
    .control-group {
      flex: 1;
      min-width: 120px;
    }
    .control-group label {
      font-weight: 500;
      margin-bottom: 0.25rem;
      display: block;
      font-size: 0.9rem;
    }
    .controls input[type="number"],
    .controls input[type="range"] {
      width: 100%;
      font-size: 1rem;
      padding: 0.5rem;
      border: 1px solid var(--border-color);
      border-radius: 8px;
    }
    #crop-result {
      margin-top: 2rem;
      text-align: center;
    }
    #crop-result h3 {
      margin-bottom: 1rem;
      font-size: 1.2rem;
      color: var(--primary-color);
    }
    #crop-result canvas {
      max-width: 100%;
      height: auto;
      border: 1px solid #ccc;
      border-radius: 12px;
    }
    footer {
      text-align: center;
      padding: 1rem;
      background: #fff;
      font-size: .9rem;
      border-top: 1px solid var(--border-color);
      margin-top: auto;
    }
    .section-title {
      font-weight: 600;
      font-size: 1.3rem;
      margin-bottom: 1rem;
      border-left: 4px solid var(--primary-color);
      padding-left: 0.5rem;
    }
    #start-interval { background-color: var(--start-color); }
    #start-interval:hover:not(:disabled) { background-color: var(--start-dark); }
    #stop-interval { background-color: var(--stop-color); }
    #stop-interval:hover:not(:disabled) { background-color: var(--stop-dark); }
  </style>
</head>
<body>
  <header>
    <h1>ESP32-CAM: Зчитування лічильника</h1>
    <img src="https://upload.wikimedia.org/wikipedia/commons/thumb/d/dc/%D0%A1%D1%83%D0%BC%D0%94%D0%A3.png/1200px-%D0%A1%D1%83%D0%BC%D0%94%D0%A3.png" alt="СумДУ Лого">
  </header>

  <main>
    <section class="card">
      <h3 class="section-title">Керування</h3>
      <div class="grid grid-2">
        <button id="capture-photo" class="btn">
          <span class="material-icons">camera_alt</span> Нове фото
        </button>
        <button id="send-aws" class="btn">
          <span class="material-icons">cloud_upload</span> Тест AWS
        </button>
        <a href="https://uqdki7wue7.execute-api.eu-central-1.amazonaws.com/dim/electricitymeter/alldata" target="_blank" class="btn">
          <span class="material-icons">analytics</span> Результати
        </a>
      </div>
    </section>

    <section class="card">
      <h3 class="section-title">Інтервальне відправлення</h3>
      <div class="grid grid-2">
        <button id="start-interval" class="btn">
          <span class="material-icons">timer</span> Старт
        </button>
        <button id="stop-interval" class="btn">
          <span class="material-icons">timer_off</span> Стоп
        </button>
      </div>
    </section>

    <section class="card">
      <h3 class="section-title">Налаштування зображення</h3>
      <img id="photo" src="saved-photo.jpg" alt="Фото з камери">
      <div class="controls">
        <div class="control-group">
          <label for="esp32-crop-left">Ліво</label>
          <input type="number" id="esp32-crop-left">
        </div>
        <div class="control-group">
          <label for="esp32-crop-top">Зверху</label>
          <input type="number" id="esp32-crop-top">
        </div>
        <div class="control-group">
          <label for="esp32-crop-width">Ширина</label>
          <input type="number" id="esp32-crop-width">
        </div>
        <div class="control-group">
          <label for="esp32-crop-height">Висота</label>
          <input type="number" id="esp32-crop-height">
        </div>
        <div class="control-group" style="flex-basis:100%;">
          <label for="rotate-image">Поворот: <span id="rotate-degrees">0</span>°</label>
          <input type="range" id="rotate-image" min="-90" max="90" step="1" value="0">
        </div>
      </div>
      <div style="margin-top:1.5rem; display:flex; flex-wrap:wrap; gap:1rem;">
        <button id="crop-button" class="btn">
          <span class="material-icons">crop</span> Перегляд
        </button>
        <button id="send-crop-data" class="btn">
          <span class="material-icons">save</span> Зберегти
        </button>
      </div>
      <div id="crop-result"></div>
    </section>
  </main>

  <footer>
    Дейнека Ілля, СумДУ, 2025
  </footer>

  <script src="https://code.jquery.com/jquery-3.7.0.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.6.0/cropper.min.js"></script>
  <script>
    document.addEventListener('DOMContentLoaded', () => {
      // --- DOM елементи ---
      const image = document.getElementById('photo');
      const capture = document.getElementById('capture-photo');
      const sendAws = document.getElementById('send-aws');
      const cropButton = document.getElementById('crop-button');
      const sendCrop = document.getElementById('send-crop-data');
      const result = document.getElementById('crop-result');
      const rotateSlider = document.getElementById('rotate-image');
      const rotateDegrees = document.getElementById('rotate-degrees');
      const startIntervalBtn = document.getElementById('start-interval');
      const stopIntervalBtn = document.getElementById('stop-interval');
      
      let cropper;

      // --- Cropper.js ініціалізація ---
      cropper = new Cropper(image, {
        zoomable: false,
        rotatable: true,
        viewMode: 1,
        ready() { cropper.zoomTo(1); },
        crop(e) {
          const d = e.detail;
          document.getElementById('esp32-crop-left').value = Math.round(d.x);
          document.getElementById('esp32-crop-top').value = Math.round(d.y);
          document.getElementById('esp32-crop-width').value = Math.round(d.width);
          document.getElementById('esp32-crop-height').value = Math.round(d.height);
        }
      });
      
      // --- Обробники подій ---

      // Оновлення рамки обрізки при зміні значень в полях
      ['left', 'top', 'width', 'height'].forEach(prop => {
        const input = document.getElementById(`esp32-crop-${prop}`);
        input.addEventListener('change', () => {
          const data = {
            x:      parseInt(document.getElementById('esp32-crop-left').value, 10),
            y:      parseInt(document.getElementById('esp32-crop-top').value, 10),
            width:  parseInt(document.getElementById('esp32-crop-width').value, 10),
            height: parseInt(document.getElementById('esp32-crop-height').value, 10)
          };
          cropper.setData(data);
        });
      });

      // Поворот зображення
      rotateSlider.oninput = () => {
        rotateDegrees.textContent = rotateSlider.value;
        cropper.rotateTo(parseInt(rotateSlider.value, 10));
      };

      // Попередній перегляд обрізки
      cropButton.onclick = () => {
        result.innerHTML = '';
        const title = document.createElement('h3');
        title.textContent = 'Результат обрізки';
        const canvas = cropper.getCroppedCanvas();
        result.append(title, canvas);
      };

      // Зберегти дані обрізки
      sendCrop.onclick = async () => {
        sendCrop.disabled = true;
        const d = cropper.getData();
        const params = new URLSearchParams({
          cropLeft:   Math.round(d.x),
          cropTop:    Math.round(d.y),
          cropWidth:  Math.round(d.width),
          cropHeight: Math.round(d.height),
          rotate:     rotateSlider.value
        });
        const res = await fetch(`/cropData?${params}`);
        alert(res.ok ? 'Дані обрізки успішно надіслано!' : 'Помилка надсилання даних обрізки.');
        sendCrop.disabled = false;
      };

      // Зробити нове фото
      capture.onclick = async () => {
        capture.disabled = true;
        capture.innerHTML = '<span class="material-icons">hourglass_top</span> Завантаження...';
        await fetch('/capture');
        await new Promise(r => setTimeout(r, 5000));
        
        try {
            const resp = await fetch('/saved-photo.jpg?t=' + new Date().getTime()); // Додано для уникнення кешування
            const blob = await resp.blob();
            const url = URL.createObjectURL(blob);
            cropper.replace(url);
            rotateSlider.value = 0;
            rotateDegrees.textContent = '0';
        } catch(error) {
            console.error("Помилка завантаження фото:", error);
            alert("Не вдалося завантажити нове фото.");
        } finally {
            capture.disabled = false;
            capture.innerHTML = '<span class="material-icons">camera_alt</span> Нове фото';
        }
      };

      // Тестове відправлення на AWS
      sendAws.onclick = async () => {
        sendAws.disabled = true;
        await fetch('/send-aws');
        alert('Команду надіслано на AWS.');
        sendAws.disabled = false;
      };

      // Старт інтервального відправлення
      startIntervalBtn.onclick = async () => {
        startIntervalBtn.disabled = true;
        startIntervalBtn.innerHTML = '<span class="material-icons">hourglass_top</span> Запуск...';
        const response = await fetch('/startInterval');
        alert(response.ok ? 'Інтервальне відправлення розпочато.' : 'Помилка запуску.');
        startIntervalBtn.disabled = false;
        startIntervalBtn.innerHTML = '<span class="material-icons">timer</span> Старт';
      };

      // Стоп інтервального відправлення
      stopIntervalBtn.onclick = async () => {
        stopIntervalBtn.disabled = true;
        stopIntervalBtn.innerHTML = '<span class="material-icons">hourglass_top</span> Зупинка...';
        const response = await fetch('/stopInterval');
        alert(response.ok ? 'Інтервальне відправлення зупинено.' : 'Помилка зупинки.');
        stopIntervalBtn.disabled = false;
        stopIntervalBtn.innerHTML = '<span class="material-icons">timer_off</span> Стоп';
      };

    });
  </script>
</body>
</html>
)rawliteral";

#endif