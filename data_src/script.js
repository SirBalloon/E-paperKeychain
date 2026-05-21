const DISPLAY_WIDTH = 296;
const DISPLAY_HEIGHT = 128;

const uploadArea = document.getElementById('uploadArea');
const fileInput = document.getElementById('fileInput');
const previewContainer = document.getElementById('previewContainer');
const previewImage = document.getElementById('previewImage');
const uploadButton = document.getElementById('uploadButton');
const status = document.getElementById('status');
const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');

let processedImageData = null;

uploadArea.addEventListener('click', () => {
    fileInput.click();
});

uploadArea.addEventListener('dragover', (e) => {
    e.preventDefault();
    uploadArea.style.borderColor = '#764ba2';
    uploadArea.style.background = '#f0f2ff';
});

uploadArea.addEventListener('dragleave', () => {
    uploadArea.style.borderColor = '#667eea';
    uploadArea.style.background = '#f8f9ff';
});

uploadArea.addEventListener('drop', (e) => {
    e.preventDefault();
    uploadArea.style.borderColor = '#667eea';
    uploadArea.style.background = '#f8f9ff';

    const files = e.dataTransfer.files;
    if (files.length > 0) {
        handleFile(files[0]);
    }
});

fileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
        handleFile(e.target.files[0]);
    }
});

function handleFile(file) {
    if (!file.type.startsWith('image/')) {
        showStatus('Please select an image file', 'error');
        return;
    }

    showStatus('Processing image...', 'processing');

    const reader = new FileReader();
    reader.onload = (e) => {
        const img = new Image();
        img.onload = () => {
            processImage(img);
        };
        img.src = e.target.result;
    };
    reader.readAsDataURL(file);
}

function processImage(img) {
    canvas.width = DISPLAY_WIDTH;
    canvas.height = DISPLAY_HEIGHT;

    const scale = Math.min(DISPLAY_WIDTH / img.width, DISPLAY_HEIGHT / img.height);
    const scaledWidth = img.width * scale;
    const scaledHeight = img.height * scale;
    const offsetX = (DISPLAY_WIDTH - scaledWidth) / 2;
    const offsetY = (DISPLAY_HEIGHT - scaledHeight) / 2;

    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    ctx.drawImage(img, offsetX, offsetY, scaledWidth, scaledHeight);

    const imageData = ctx.getImageData(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    const dithered = floydSteinbergDithering(imageData);

    ctx.putImageData(dithered, 0, 0);
    previewImage.src = canvas.toDataURL();
    previewContainer.style.display = 'block';

    processedImageData = packImageData(dithered);

    uploadButton.disabled = false;
    showStatus('Image processed! Ready to upload', 'success');
}

function floydSteinbergDithering(imageData) {
    const data = imageData.data;
    const width = imageData.width;
    const height = imageData.height;

    for (let i = 0; i < data.length; i += 4) {
        const gray = 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
        data[i] = data[i + 1] = data[i + 2] = gray;
    }

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const idx = (y * width + x) * 4;
            const oldPixel = data[idx];
            const newPixel = oldPixel < 128 ? 0 : 255;
            const error = oldPixel - newPixel;

            data[idx] = data[idx + 1] = data[idx + 2] = newPixel;

            if (x + 1 < width) {
                data[idx + 4] = Math.max(0, Math.min(255, data[idx + 4] + error * 7 / 16));
            }
            if (y + 1 < height) {
                if (x > 0) {
                    data[idx + (width * 4) - 4] += error * 3 / 16;
                }
                data[idx + (width * 4)] += error * 5 / 16;
                if (x + 1 < width) {
                    data[idx + (width * 4) + 4] += error * 1 / 16;
                }
            }
        }
    }

    return imageData;
}

function packImageData(imageData) {
    const data = imageData.data;
    const width = imageData.width;
    const height = imageData.height;

    const bytesPerRow = Math.ceil(width / 8);
    const totalBytes = bytesPerRow * height;
    const packed = new Uint8Array(totalBytes);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const idx = (y * width + x) * 4;
            const pixel = data[idx];

            if (pixel < 128) {
                const byteIdx = y * bytesPerRow + Math.floor(x / 8);
                const bitIdx = 7 - (x % 8);
                packed[byteIdx] |= (1 << bitIdx);
            }
        }
    }

    return packed;
}

uploadButton.addEventListener('click', async () => {
    if (!processedImageData) return;

    uploadButton.disabled = true;
    uploadButton.textContent = 'Uploading...';
    showStatus('Sending to display...', 'processing');

    try {
        const response = await fetch('/upload', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/octet-stream'
            },
            body: processedImageData
        });

        if (response.ok) {
            showStatus('Image displayed successfully!', 'success');
            uploadButton.textContent = 'Uploaded!';
            setTimeout(() => {
                uploadButton.textContent = 'Upload Another';
                uploadButton.disabled = false;
            }, 2000);
        } else {
            showStatus('Upload failed. Please try again.', 'error');
            uploadButton.disabled = false;
            uploadButton.textContent = 'Upload to Display';
        }
    } catch (error) {
        showStatus('Connection error: ' + error.message, 'error');
        uploadButton.disabled = false;
        uploadButton.textContent = 'Upload to Display';
    }
});

function showStatus(message, type) {
    status.textContent = message;
    status.className = 'status ' + type;
}
