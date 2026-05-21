Things to improve:
    
    Image Quality:
    - Add Red Channel Support
    - Better Dithering options
    - Brightness/contrast Adjustments

    // This silently drops data if buffer overflows — no recovery
    if (imageBuffer != nullptr && (totalReceived + len) <= IMAGE_BUFFER_SIZE) {
        memcpy(imageBuffer + totalReceived, data, len);
    }

    Reliability & Error Handling:
    - You'd want to send an error back to the browser and reset state properly. Similarly, malloc can fail silently — the display just does nothing with no feedback to the user.
    - Upload validation is also missing — currently anyone on the network can POST arbitrary data to /upload. A simple password check on the upload endpoint would help.

    User Experience:
    - Focus on progress feedback displaying accurately on the webserver
    - Image Orientation controls
    - Better preview accuracy -: canvas preview doesn't account for the e-paper contrast characteristics

    Architecture:
    - Presistant Storage: Save to LittleFS (ESP32 internal flash filesystem) so the previous image can redraw on startup
    - Chunked Rendering: Current pixel by pixel loop is slow. The GxEDP2 library supports writing raw bitmap buffers directly which would be significantly faster
    - mDNS: Instead of using the Ip address, Setting up an mDNS allows them to visit a string based URL. Making it much friendlier

    Security:
    - Wifi Password should be protected and properly secure.
    - No rate limiting on /Upload
    - Consider adding a simple token or pin at the upload endpoint

Core Issue with E-Paper displays:
- 296 x 128 Pixels is very low resolution 
- Only three colours with no blending
- No backlight so contrast depends entirely on ink density

Higher quality means making really smart trade-offs and designing the software to handle many different situations

1. Better Input Preprocessing -
    Contrast stretching — most photos are "flat" with their darkest pixel being dark grey and brightest being light grey, not true black/white. Stretching the histogram to fill the full 0-255 range dramatically improves output:
        Find darkest pixel value in image → map to 0
        Find brightest pixel value → map to 255
        Stretch everything in between proportionally
    
    Sharpening — dithering softens edges. Running an unsharp mask before dithering preserves edge definition. The concept is:
        Sharpened = Original + amount × (Original - Blurred)
    You subtract a blurred version from the original to find edges, then add them back stronger.
    
    Gamma correction — human eyes don't perceive brightness linearly. E-paper's physical ink also has nonlinear response. Applying a gamma curve before dithering means your dither patterns better match perceived brightness rather than raw numbers.

2. Better scaling
    Your current code uses the browser's default canvas scaling which is just bilinear interpolation — fine but not optimal.

    Lanczos resampling is the gold standard for downscaling — it preserves sharpness much better. The concept is it uses a mathematical windowing function that considers more neighboring pixels and weights them intelligently rather than just averaging nearby ones.

    The practical impact is significant when shrinking a large photo down to 296×128 — you get crisper edges and less blurring.
    Aspect ratio handling also matters more than it seems. Your current code letterboxes (adds white bars). Alternatives worth considering:

    - Crop to fill — no wasted pixels, loses edges
    - Seam carving — content-aware resizing that removes "boring" pixels first, preserves subjects
    - Let user pan/zoom before sending

3. Better Dithering Algorithm
    Floyd-Steinberg is good but not the best for every image type.

    Atkinson dithering — only propagates 3/4 of the error instead of all of it. This sounds worse but actually produces cleaner results on low resolution displays because it creates tighter, more defined dot clusters rather than spreading noise everywhere. Old Mac computers used this.
        Distributes error to 6 neighbors at 1/8 each
        Deliberately "loses" 2/8 of error
        Result: cleaner highlights, crisper edges

    Ordered dithering (Bayer matrix) — instead of propagating error, you compare each pixel against a fixed threshold pattern. Produces a regular crosshatch look that some people prefer for graphics and logos. Very fast to compute.
    
    Blue noise dithering — the most perceptually pleasing modern approach. Regular dithering creates structured patterns your eye picks up as texture. Blue noise randomizes the dot placement in a way that looks more organic. Harder to implement but noticeably better results.
    
    The key insight is different algorithms suit different content:
        Content Type to Best Algorithm
        Photos = Atkinson or Blue noise
        Logos/graphics = Ordered (Bayer)
        Text = No dithering hard threshold 
        Line art = Hard threshold

4. Content-Aware Processing
    This is the biggest quality jump available to you — treating different parts of the image differently.

    Edge detection first — find where edges are in the image, then apply a hard threshold there instead of dithering. Dithering on edges creates fuzzy outlines; hard thresholding keeps them crisp. On smooth areas away from edges, use dithering freely.

    Text detection — if the image contains text, dithering destroys thin strokes. You'd want to identify text regions and apply a high-contrast hard threshold specifically there.

    Local adaptive thresholding — instead of one global threshold (128), calculate the threshold for each pixel based on its surrounding neighborhood. This handles images with uneven lighting dramatically better.

    For each pixel:
        Look at surrounding N×N area
        Calculate local average brightness
        Use that local average as the threshold
        
    Result: 
        dark areas of image still get detail
        bright areas still get detail
        not crushed to all-black or all-white

Potential order within the algo:
    1. Resize FIRST (Lanczos)
            ↓
    2. Contrast stretch
            ↓
    3. Gamma correction
            ↓
    4. Sharpen (after resize, not before)
            ↓
    5. Edge detection (save edge map)
            ↓
    6. Dither non-edge regions
            ↓
    7. Hard threshold edge regions
            ↓
    8. Pack bits