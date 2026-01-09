#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Name Tag Remote</title>
  <style>
    body{font-family:Arial;padding:18px;max-width:900px}
    h2{margin:0 0 10px 0}
    .row{margin:14px 0}
    button{padding:10px 12px;margin:6px 8px 0 0;font-size:14px;cursor:pointer}
    input, select{padding:8px;font-size:14px}
    table{border-collapse:collapse;width:100%;margin-top:10px}
    th, td{border:1px solid #ddd;padding:8px;text-align:left}
    th{background:#f6f6f6}
    .muted{color:#666;font-size:13px}
    #log{background:#f4f4f4;padding:10px;border-radius:10px;min-height:90px;white-space:pre-wrap;margin-top:12px}
    .pill{display:inline-block;padding:3px 8px;border-radius:999px;background:#eee;font-size:12px;margin-left:6px}

    /* NEW: offline UI */
    .offline { opacity: 0.35; }
    .badge{
      display:inline-block;
      padding:2px 8px;
      border-radius:999px;
      font-size:12px;
      background:#eee;
      margin-left:8px;
    }
    .badge.offline{ background:#ffd6d6; }
  </style>
</head>
<body>
  <h2>Name Tag Remote</h2>
  <div class="muted">Auto-refresh keeps your selections. Offline tags stay visible (faded) so you can still click them.</div>

  <div class="row">
    <button onclick="refreshDevices(true)">Refresh now</button>
    <span class="pill" id="countPill">0 online / 0 total</span>
  </div>

  <div class="row">
    <b>Apply to SELECTED tags:</b><br/>
    <button onclick="sendToSelected(0)">0 OFF</button>
    <button onclick="sendToSelected(1)">1</button>
    <button onclick="sendToSelected(2)">2</button>
    <button onclick="sendToSelected(3)">3</button>
    <button onclick="sendToSelected(4)">4</button>
    <button onclick="selectAll(true)">Select all</button>
    <button onclick="selectAll(false)">Clear</button>
  </div>

  <div class="row">
    <b>Groups (stored in this browser):</b><br/>
    <input id="groupName" placeholder="Group name (e.g., Team A)" />
    <button onclick="saveGroup()">Save group from SELECTED</button>
    <select id="groupSelect"></select>
    <button onclick="loadGroup()">Load group</button>
    <button onclick="deleteGroup()">Delete group</button>
  </div>

  <div class="row">
    <b>Devices:</b>
    <table>
      <thead>
        <tr>
          <th style="width:60px;">Pick</th>
          <th>MAC</th>
          <th style="width:420px;">Quick control (single tag)</th>
        </tr>
      </thead>
      <tbody id="devBody">
        <tr><td colspan="3">Loading...</td></tr>
      </tbody>
    </table>
  </div>

  <div id="log">Ready.</div>

<script>
  const log = (m) => document.getElementById("log").textContent = m;
  const devBody = document.getElementById("devBody");
  const countPill = document.getElementById("countPill");
  const groupSelect = document.getElementById("groupSelect");

  // Persist selections across refresh
  const selected = new Set(); // MAC strings

  // Client-side cache: keep devices even if /devices temporarily misses them
  // known[mac] = { lastSeen: ms, online: bool }
  const known = new Map();

  // If a device hasn't been seen for this long, remove it entirely from UI
  // (offline but kept for control for some time)
  const OFFLINE_REMOVE_MS = 10 * 60 * 1000; // 10 minutes

  function nowMs(){ return Date.now(); }

  function getGroups(){
    try { return JSON.parse(localStorage.getItem("tag_groups") || "{}"); }
    catch(e){ return {}; }
  }
  function setGroups(g){
    localStorage.setItem("tag_groups", JSON.stringify(g));
  }
  function refreshGroupDropdown(){
    const groups = getGroups();
    groupSelect.innerHTML = "";
    const keys = Object.keys(groups);
    if(keys.length === 0){
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "(no groups yet)";
      groupSelect.appendChild(opt);
      return;
    }
    keys.forEach(k => {
      const opt = document.createElement("option");
      opt.value = k;
      opt.textContent = k + ` (${groups[k].length})`;
      groupSelect.appendChild(opt);
    });
  }

  function sortedDeviceList(){
    const arr = [];
    for (const [mac, meta] of known.entries()) {
      arr.push({ mac, online: meta.online, lastSeen: meta.lastSeen });
    }
    // Online first, offline last; newest first in each group
    arr.sort((a,b) => {
      if (a.online !== b.online) return a.online ? -1 : 1;
      return b.lastSeen - a.lastSeen;
    });
    return arr;
  }

  async function refreshDevices(manual=false){
    try{
      const r = await fetch("/devices");
      const macs = await r.json();

      const t = nowMs();
      const seenSet = new Set(macs);

      // Update devices seen now
      macs.forEach(mac => {
        const cur = known.get(mac) || { lastSeen: 0, online: false };
        cur.lastSeen = t;
        cur.online = true;
        known.set(mac, cur);
      });

      // Mark devices not seen now as offline (do NOT remove)
      for (const [mac, meta] of known.entries()) {
        if (!seenSet.has(mac)) {
          meta.online = false;
          known.set(mac, meta);
        }
      }

      // Optional: remove devices that have been offline too long
      for (const [mac, meta] of known.entries()) {
        if (!meta.online && (t - meta.lastSeen) > OFFLINE_REMOVE_MS) {
          known.delete(mac);
          selected.delete(mac);
        }
      }

      renderTable(sortedDeviceList());

      const onlineCount = macs.length;
      const totalCount = known.size;
      countPill.textContent = `${onlineCount} online / ${totalCount} total`;

      if(manual) log(`Refreshed. Online ${onlineCount}, total ${totalCount}.`);
    }catch(e){
      log("ERROR loading devices: " + e);
    }
  }

  function renderTable(list){
    devBody.innerHTML = "";
    if(list.length === 0){
      devBody.innerHTML = `<tr><td colspan="3">No tags yet.</td></tr>`;
      return;
    }

    list.forEach((item, idx) => {
      const mac = item.mac;
      const checked = selected.has(mac) ? "checked" : "";
      const offlineClass = item.online ? "" : "offline";
      const statusBadge = item.online
        ? `<span class="badge">online</span>`
        : `<span class="badge offline">offline</span>`;

      const tr = document.createElement("tr");
      tr.className = offlineClass;

      tr.innerHTML = `
        <td>
          <input type="checkbox" ${checked} onchange="togglePick('${mac}', this.checked)" />
        </td>
        <td>
          <b>Tag</b> ${statusBadge}<br/>
          <span class="muted">${mac}</span>
        </td>
        <td>
          <button onclick="sendOne('${mac}',0)">0 OFF</button>
          <button onclick="sendOne('${mac}',1)">1</button>
          <button onclick="sendOne('${mac}',2)">2</button>
          <button onclick="sendOne('${mac}',3)">3</button>
          <button onclick="sendOne('${mac}',4)">4</button>
        </td>
      `;
      devBody.appendChild(tr);
    });
  }

  window.togglePick = (mac, isOn) => {
    if(isOn) selected.add(mac);
    else selected.delete(mac);
    log(`Selected: ${Array.from(selected).length} tag(s).`);
  };

  function selectAll(on){
    if(on){
      for (const mac of known.keys()) selected.add(mac);
    }else{
      selected.clear();
    }
    renderTable(sortedDeviceList());
    log(on ? "Selected all (including offline)." : "Cleared selection.");
  }

  async function sendOne(mac, c){
    try{
      const r = await fetch(`/set?t=${encodeURIComponent(mac)}&c=${c}`);
      log(await r.text());
    }catch(e){
      log("ERROR sending single: " + e);
    }
  }

  async function sendToSelected(c){
    const list = Array.from(selected);
    if(list.length === 0){
      log("No tags selected.");
      return;
    }
    log(`Sending pattern ${c} to ${list.length} selected tag(s)...`);

    for(const mac of list){
      try{
        await fetch(`/set?t=${encodeURIComponent(mac)}&c=${c}`);
      }catch(e){
        // keep going
      }
      await new Promise(res => setTimeout(res, 20));
    }
    log(`Done. Pattern ${c} sent to ${list.length} tag(s).`);
  }

  function saveGroup(){
    const name = document.getElementById("groupName").value.trim();
    const list = Array.from(selected);
    if(!name){ log("Group name is empty."); return; }
    if(list.length === 0){ log("Select tags first, then save group."); return; }

    const groups = getGroups();
    groups[name] = list;
    setGroups(groups);
    refreshGroupDropdown();
    log(`Saved group "${name}" with ${list.length} tag(s).`);
  }

  function loadGroup(){
    const key = groupSelect.value;
    const groups = getGroups();
    if(!key || !groups[key]){ log("No group selected."); return; }

    selected.clear();
    groups[key].forEach(m => selected.add(m));
    renderTable(sortedDeviceList());
    log(`Loaded group "${key}" (${groups[key].length} tag(s)).`);
  }

  function deleteGroup(){
    const key = groupSelect.value;
    const groups = getGroups();
    if(!key || !groups[key]){ log("No group selected."); return; }
    delete groups[key];
    setGroups(groups);
    refreshGroupDropdown();
    log(`Deleted group "${key}".`);
  }

  refreshGroupDropdown();
  refreshDevices(true);
  setInterval(refreshDevices, 2500);
</script>
</body>
</html>
)HTML";
