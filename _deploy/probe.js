// Probe VPS - learn current state
const { Client } = require('ssh2');

const cfg = {
    host: '31.42.127.82',
    port: 22,
    username: 'root',
    password: 'eyg!nctW@V7uYFNJ',
    readyTimeout: 20000,
};

function exec(conn, cmd) {
    return new Promise((res, rej) => {
        conn.exec(cmd, (err, stream) => {
            if (err) return rej(err);
            let out = '', errOut = '';
            stream.on('data', d => out += d.toString());
            stream.stderr.on('data', d => errOut += d.toString());
            stream.on('close', (code) => res({ code, out, err: errOut }));
        });
    });
}

const cmds = [
    'cat /etc/os-release | head -3',
    'node --version 2>&1 || echo NO_NODE',
    'npm --version 2>&1 || echo NO_NPM',
    'pm2 --version 2>&1 || echo NO_PM2',
    'systemctl --version 2>&1 | head -1',
    'pm2 list 2>&1 | head -30',
    'systemctl list-units --type=service --state=running 2>&1 | grep -iE "volaura|signaling|deeptalk|node" | head',
    'find /root /home /opt /srv -maxdepth 4 -type f -name server.js 2>/dev/null | head -10',
    'find /root /home /opt /srv -maxdepth 4 -type d -name vps-server 2>/dev/null | head -5',
    'find /root /home /opt /srv -maxdepth 4 -type d -name volaura 2>/dev/null | head -5',
    'ss -ltnp 2>/dev/null | head -20',
];

const conn = new Client();
conn.on('ready', async () => {
    console.log('[OK] SSH bağlandı');
    for (const c of cmds) {
        try {
            const r = await exec(conn, c);
            console.log(`\n$ ${c}`);
            if (r.out) console.log(r.out.trim());
            if (r.err) console.log('STDERR:', r.err.trim());
        } catch (e) {
            console.log(`\n$ ${c}\nEXEC ERROR:`, e.message);
        }
    }
    conn.end();
}).on('error', (e) => {
    console.error('[ERR]', e.message);
    process.exit(1);
}).connect(cfg);
