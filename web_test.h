#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Tag Test</title>
  <style>
    body{font-family:Arial;display:flex;flex-direction:column;gap:12px;justify-content:center;align-items:center;height:100vh}
    button{font-size:22px;padding:18px 32px;border:0;border-radius:14px;cursor:pointer;background:#222;color:#fff}
    button.on{background:#1aa34a}
    #status{font-size:13px;color:#444}
  </style>
</head>
<body>
  <button id="btn" onclick="toggle()">GREEN: OFF</button>
  <div id="status">Ready</div>

<script>
  let isOn = false;
  const btn = document.getElementById("btn");
  const status = document.getElementById("status");

  const sleep = (ms) => new Promise(r => setTimeout(r, ms));

  async function toggle(){
    isOn = !isOn;
    const c = isOn ? 2 : 0;

    try{
      status.textContent = "Loading devices...";
      const r = await fetch("/devices", { cache: "no-store" });
      const macs = await r.json();

      status.textContent = `Sending c=${c} to ${macs.length} tag(s)...`;

      for (const mac of macs) {
        // your controller already supports /set?t=MAC&c=X
        await fetch(`/set?t=${encodeURIComponent(mac)}&c=${c}`, { cache: "no-store" });
        await sleep(20);
      }

      status.textContent = `Done. Sent c=${c} to ${macs.length} tag(s).`;
    }catch(e){
      status.textContent = "Error: " + e;
    }

    btn.textContent = isOn ? "GREEN: ON" : "GREEN: OFF";
    btn.className = isOn ? "on" : "";
  }
</script>
</body>
</html>
)HTML";
