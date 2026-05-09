const { Client } = require('ssh2');
const cfg = { host:'31.42.127.82', port:22, username:'root', password:'eyg!nctW@V7uYFNJ', readyTimeout:20000 };
const conn = new Client();
const cmd = `node -e "
const fs = require('fs');
const p = '/root/deeptalk-server/.env';
const txt = fs.readFileSync(p, 'utf8').split(/\\r?\\n/);
const out = txt.map(l => l.replace(/^BASE_URL=.*/, 'BASE_URL=https://volaura.xyz'));
fs.writeFileSync(p, out.join('\\n'));
console.log('BASE_URL -> https://volaura.xyz');
" && pm2 restart volaura --update-env && sleep 2 && pm2 logs volaura --lines 5 --nostream | tail -15`;

conn.on('ready', () => {
    conn.exec(cmd, (e, s) => {
        if (e) { console.error(e); process.exit(1); }
        s.on('data', d => process.stdout.write(d));
        s.stderr.on('data', d => process.stderr.write(d));
        s.on('close', () => conn.end());
    });
}).on('error', e => { console.error(e.message); process.exit(1); }).connect(cfg);
