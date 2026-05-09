const { Client } = require('ssh2');
const cfg = { host:'31.42.127.82', port:22, username:'root', password:'eyg!nctW@V7uYFNJ', readyTimeout:20000 };
const conn = new Client();
conn.on('ready', () => {
    conn.exec('cat /root/deeptalk-server/.env; echo "---NGINX---"; cat /etc/nginx/sites-enabled/volaura* 2>/dev/null; echo "---qzz---"; cat /etc/nginx/sites-enabled/* 2>/dev/null | grep -A5 "volaura.qzz"; echo "---PORTS---"; grep -E "WSS_PORT|WS_PORT|BASE_URL" /root/deeptalk-server/.env',
    (err, stream) => {
        if (err) { console.error(err); process.exit(1); }
        stream.on('data', d => process.stdout.write(d));
        stream.stderr.on('data', d => process.stderr.write(d));
        stream.on('close', () => conn.end());
    });
}).on('error', e => { console.error(e.message); process.exit(1); }).connect(cfg);
