const { Client } = require('ssh2');
const cfg = { host:'31.42.127.82', port:22, username:'root', password:'eyg!nctW@V7uYFNJ', readyTimeout:20000 };
function exec(conn, cmd) { return new Promise((res, rej) => {
    conn.exec(cmd, (err, stream) => { if (err) return rej(err);
        let out=''; let er='';
        stream.on('data', d => out += d.toString());
        stream.stderr.on('data', d => er += d.toString());
        stream.on('close', code => res({ code, out, er }));
    });
});}
const cmds = [
    'ls -la /root/deeptalk-server/',
    'cat /root/deeptalk-server/package.json',
    'cat /root/deeptalk-server/.env 2>/dev/null | sed "s/=.*$/=***/"',
    'pm2 show volaura 2>&1 | head -30',
    'cat /etc/nginx/sites-enabled/* 2>&1 | head -120',
    'ls /etc/letsencrypt/live/ 2>/dev/null',
    'curl -s -o /dev/null -w "qzz_io_https=%{http_code}\\n" https://volaura.qzz.io/health',
    'curl -s -o /dev/null -w "xyz_https=%{http_code}\\n" https://volaura.xyz/health 2>&1',
    'dig +short volaura.xyz @8.8.8.8',
    'dig +short volaura.xyz',
    'cat /root/deeptalk-server/db.js 2>/dev/null | head -10',
    'find /root/deeptalk-server -maxdepth 2 -type f -name "*.js" 2>/dev/null',
];
const conn = new Client();
conn.on('ready', async () => {
    for (const c of cmds) {
        const r = await exec(conn, c).catch(e => ({ er: e.message, out: '' }));
        console.log(`\n$ ${c}`);
        if (r.out) console.log(r.out.trim());
        if (r.er) console.log('STDERR:', r.er.trim());
    }
    conn.end();
}).on('error', e => { console.error('[ERR]', e.message); process.exit(1); }).connect(cfg);
