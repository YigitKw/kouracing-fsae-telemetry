const TEAM_PASSWORD = process.env.TEAM_PASSWORD || "kou2026";

if (TEAM_PASSWORD === "kou2026") {
    console.warn("[AUTH] Using default team password");
}

function handleAuthConnection(wss, ws) {
    let authenticated = false;

    const authTimeout = setTimeout(() => {
        if (!authenticated) {
            ws.send("auth_error:timeout");
            ws.close();
        }
    }, 10000);

    ws.on('message', (message) => {
        if (Buffer.isBuffer(message) || message instanceof ArrayBuffer) {
            return;
        }

        const text = message.toString();
        if (!authenticated) {
            if (text === TEAM_PASSWORD) {
                authenticated = true;
                clearTimeout(authTimeout);
                ws.send("auth_success");
                console.log("[AUTH] Client authenticated");
            } else {
                console.log("[AUTH] Invalid credentials");
                ws.send("auth_error:wrong_password");
                ws.close();
            }
        }
    });

    ws.on('close', () => clearTimeout(authTimeout));
    ws.on('error', console.error);

    Object.defineProperty(ws, 'authenticated', {
        get: () => authenticated,
    });
}

module.exports = { handleAuthConnection };
