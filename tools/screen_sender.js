const net = require('net');
const screenshot = require('screenshot-desktop');
const sharp = require('sharp');

const ESP_IP = process.argv[2] || '192.168.43.150';
const FPS = parseInt(process.argv[3]) || 15;
const INTERVAL = Math.floor(1000 / FPS);

const W = 240, H = 240, SIZE = W * H * 2;
let sock = null;
let connecting = false;

function connect() {
    if (connecting) return;
    connecting = true;
    sock = new net.Socket();
    sock.setNoDelay(true);
    sock.connect(8888, ESP_IP, () => {
        console.log('Connected to ' + ESP_IP + ':8888');
        connecting = false;
    });
    sock.on('error', () => { sock.destroy(); connecting = false; });
    sock.on('close', () => { sock = null; connecting = false; });
}

async function sendFrame() {
    if (!sock || sock.destroyed) { connect(); return; }
    try {
        const png = await screenshot({ format: 'png' });
        // sharp: native C, much faster than Jimp
        const raw = await sharp(png).resize(W, H).ensureAlpha().raw().toBuffer();

        const frame = Buffer.allocUnsafe(SIZE);
        for (let i = 0, j = 0; i < raw.length; i += 4, j += 2) {
            const v = ((raw[i] & 0xF8) << 8) | ((raw[i+1] & 0xFC) << 3) | (raw[i+2] >> 3);
            frame[j] = v & 0xFF;
            frame[j + 1] = (v >> 8) & 0xFF;
        }
        sock.write(frame);
    } catch (e) { /* skip */ }
}

setInterval(sendFrame, INTERVAL);
connect();
console.log('Streaming ' + FPS + 'fps -> ' + ESP_IP + ':8888');
