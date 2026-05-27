const DISPLAY_WIDTH = 296;
const DISPLAY_HEIGHT = 128;

const uploadArea = document.getElementById('uploadArea');
const fileInput = document.getElementById('fileInput');
const previewContainer = document.getElementById('previewContainer');
const previewImage = document.getElementById('previewImage');
const uploadButton = document.getElementById('uploadButton');
const status = document.getElementById('status');
const canvas = document.getElementById('canvas');
const gammaSlider = document.getElementById('gammaSlider');
const sharpnessSlider = document.getElementById('sharpnessSlider');
const edgeSlider = document.getElementById('edgeSlider');
const controls = document.getElementById('controls');
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

function sinc(x) {
    if (x === 0) {
        return 1;
    } else {
        return Math.sin(Math.PI * x) / (Math.PI * x);
    }
}

function lanczos(x) {
    if (Math.abs(x) >= 2) {
        return 0;
    } else {
        return sinc(x) * sinc(x / 2);
    }
}

function lanczosResampling(img) {
    const offscreen = document.createElement('canvas');
    const offCtx = offscreen.getContext('2d');

    offscreen.width = img.width;
    offscreen.height = img.height;

    offCtx.drawImage(img, 0, 0);
    const srcData = offCtx.getImageData(0, 0, img.width, img.height).data;

    const output = new Uint8ClampedArray(DISPLAY_WIDTH * DISPLAY_HEIGHT * 4);
    const scale = Math.max(DISPLAY_WIDTH / img.width, DISPLAY_HEIGHT / img.height);
    const scaledW = img.width * scale;
    const scaledH = img.height * scale;
    const offsetX = (DISPLAY_WIDTH - scaledW) / 2;
    const offsetY = (DISPLAY_HEIGHT - scaledH) / 2;

    for (let oy = 0; oy < DISPLAY_HEIGHT; oy++) {
        for (let ox = 0; ox < DISPLAY_WIDTH; ox++) {
            const sx = (ox - offsetX) / scale;
            const sy = (oy - offsetY) / scale;

            let r = 0, g = 0, b = 0, totalWeight = 0;

            for (let ky = -1; ky <= 2; ky++) {
                for (let kx = -1; kx <= 2; kx++) {
                    const px = Math.floor(sx) + kx;
                    const py = Math.floor(sy) + ky;
                    if (px >= 0 && px < img.width && py >= 0 && py < img.height) {
                        const weight = lanczos(sx - px) * lanczos(sy - py);
                        const srcIdx = (py * img.width + px) * 4;
                        r += srcData[srcIdx] * weight;
                        g += srcData[srcIdx + 1] * weight;
                        b += srcData[srcIdx + 2] * weight;
                        totalWeight += weight;
                    }
                }
            }
            const outIdx = (oy * DISPLAY_WIDTH + ox) * 4;
            output[outIdx] = Math.max(0, Math.min(255, r / totalWeight));
            output[outIdx + 1] = Math.max(0, Math.min(255, g / totalWeight));
            output[outIdx + 2] = Math.max(0, Math.min(255, b / totalWeight));
            output[outIdx + 3] = 255;
        }
    }

    return new ImageData(output, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

function processImage(img) {
    canvas.width = DISPLAY_WIDTH;
    canvas.height = DISPLAY_HEIGHT;

    const gamma = parseFloat(gammaSlider.value);
    const sharpness = parseFloat(sharpnessSlider.value);
    const edgeThreshold = parseInt(edgeSlider.value);

    const imageData = lanczosResampling(img);
    const gray = Grayscale(imageData);
    const stretched = contrastStretch(gray)
    const gammaCorrected = gammaCorrection(stretched, gamma);
    const Sharpenedmask = unSharpMask(gammaCorrected, sharpness);
    const edgeMap = detectEdges(Sharpenedmask);
    const dithered = AtkinsonDithering(Sharpenedmask, edgeMap, edgeThreshold);

    ctx.putImageData(dithered, 0, 0);
    previewImage.src = canvas.toDataURL();
    previewContainer.style.display = 'block';
    controls.style.display = 'block';
    processedImageData = packImageData(dithered);

    uploadButton.disabled = false;
    showStatus('Image processed! Ready to upload', 'success');
}

function Grayscale(imageData) {
    const data = imageData.data;

    for (let i = 0; i < data.length; i += 4) {
        const gray = 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
        data[i] = data[i + 1] = data[i + 2] = gray;
    }

    return imageData;
}

function contrastStretch(imageData) {
    const data = imageData.data;
    let min = 255;
    let max = 0;

    for (let i = 0; i < data.length; i += 4) {
        if (data[i] < min) {
            min = data[i];
        }
        if (data[i] > max) {
            max = data[i];
        }
    }

    if (min === max) {
        return imageData;
    }

    for (let i = 0; i < data.length; i += 4) {
        data[i] = data[i + 1] = data[i + 2] = (data[i] - min) / (max - min) * 255;
    }

    return imageData;
}

function gammaCorrection(imageData, gamma) {
    const data = imageData.data;

    for (let i = 0; i < data.length; i += 4) {
        data[i] = data[i + 1] = data[i + 2] = Math.pow(data[i] / 255, gamma) * 255;
    }

    return imageData;
}

function unSharpMask(imageData, amount) {
    const data = imageData.data
    const width = imageData.width;
    const height = imageData.height;
    const blurred = ctx.createImageData(imageData.width, imageData.height);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            let sum = 0;
            let count = 0;
            for (let dy = -1; dy <= 1; dy++) {
                for (let dx = -1; dx <= 1; dx++) {
                    const nx = x + dx;
                    const ny = y + dy;

                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        const nIdx = (ny * width + nx) * 4;
                        sum += data[nIdx];
                        count++;
                    }
                }
            }
            const idx = (y * width + x) * 4;
            blurred.data[idx] = blurred.data[idx + 1] = blurred.data[idx + 2] = sum / count;
            blurred.data[idx + 3] = 255;
        }
    }

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const idx = (y * width + x) * 4;
            const newValue = Math.max(0, Math.min(255, data[idx] + amount * (data[idx] - blurred.data[idx])));
            data[idx] = data[idx + 1] = data[idx + 2] = newValue;
        }
    }

    return imageData;
}

function detectEdges(imageData) {
    const data = imageData.data;
    const width = imageData.width;
    const height = imageData.height;
    const edgeMap = new Float32Array(width * height);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            let gx = 0;
            let gy = 0;
            for (let dy = -1; dy <= 1; dy++) {
                for (let dx = -1; dx <= 1; dx++) {
                    const nx = x + dx;
                    const ny = y + dy;

                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        const pixel = data[(ny * width + nx) * 4];
                        gx += pixel * dx;
                        gy += pixel * dy;
                    }
                }
            }
            edgeMap[y * width + x] = Math.sqrt(gx * gx + gy * gy);
        }
    }

    return edgeMap;
}

function AtkinsonDithering(imageData, edgeMap, edgeThreshold) {
    const data = imageData.data;
    const width = imageData.width;
    const height = imageData.height;

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const idx = (y * width + x) * 4;
            const oldPixel = data[idx];
            const newPixel = oldPixel < 128 ? 0 : 255;
            const edgeStr = edgeMap[y * width + x];
            const error = oldPixel - newPixel;

            if (edgeStr > edgeThreshold) {
                data[idx] = data[idx + 1] = data[idx + 2] = data[idx] < 128 ? 0 : 255;
            } else {
                data[idx] = data[idx + 1] = data[idx + 2] = newPixel;

                if (x + 1 < width)
                    data[idx + 4] = Math.max(0, Math.min(255, data[idx + 4] + error / 8));
                if (x + 2 < width)
                    data[idx + 8] = Math.max(0, Math.min(255, data[idx + 8] + error / 8));

                if (y + 1 < height) {
                    if (x > 0)
                        data[idx + (width * 4) - 4] = Math.max(0, Math.min(255, data[idx + (width * 4) - 4] + error / 8));
                    data[idx + (width * 4)] = Math.max(0, Math.min(255, data[idx + (width * 4)] + error / 8));
                    if (x + 1 < width)
                        data[idx + (width * 4) + 4] = Math.max(0, Math.min(255, data[idx + (width * 4) + 4] + error / 8));
                }

                if (y + 2 < height) {
                    data[idx + (width * 8)] = Math.max(0, Math.min(255, data[idx + (width * 8)] + error / 8));
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

gammaSlider.addEventListener('input', () => {
    document.getElementById('gammaValue').textContent = gammaSlider.value;
    if (currentImg) processImage(currentImg);
});

sharpnessSlider.addEventListener('input', () => {
    document.getElementById('sharpnessValue').textContent = sharpnessSlider.value;
    if (currentImg) processImage(currentImg);
});

edgeSlider.addEventListener('input', () => {
    document.getElementById('edgeValue').textContent = edgeSlider.value;
    if (currentImg) processImage(currentImg);
});

function showStatus(message, type) {
    status.textContent = message;
    status.className = 'status ' + type;
}
