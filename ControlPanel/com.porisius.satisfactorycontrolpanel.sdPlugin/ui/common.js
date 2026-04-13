let websocket = null;
let currentSettings = {};
let piUuid = null;

function connectElgatoStreamDeckSocket(inPort, inUUID, inRegisterEvent, _inInfo, inActionInfo) {
  piUuid = inUUID;
  websocket = new WebSocket(`ws://127.0.0.1:${inPort}`);

  websocket.onopen = function () {
    updateInspectorStatus("Bridge connected");

    websocket.send(JSON.stringify({
      event: inRegisterEvent,
      uuid: inUUID
    }));

    websocket.send(JSON.stringify({
      event: "getSettings",
      context: piUuid
    }));
  };

  websocket.onmessage = function (evt) {
    const msg = JSON.parse(evt.data);

    updateInspectorStatus(`Received event: ${msg.event}`);

    if (msg.event === "didReceiveSettings") {
      currentSettings = msg.payload?.settings || {};

      window.dispatchEvent(new CustomEvent("scp:settings", {
        detail: currentSettings
      }));
    }
  };

  websocket.onerror = function () {
    updateInspectorStatus("Bridge websocket error");
  };

  websocket.onclose = function () {
    updateInspectorStatus("Bridge websocket closed");
  };
}

function setSettings(settings) {
  currentSettings = { ...currentSettings, ...settings };

  if (!websocket || websocket.readyState !== WebSocket.OPEN) {
    updateInspectorStatus("Bridge not open");
    return;
  }

  websocket.send(JSON.stringify({
    event: "setSettings",
    context: piUuid,
    payload: currentSettings
  }));

  updateInspectorStatus("Sent setSettings");
}

function getSavedSettings() {
  return currentSettings;
}

function applySettingsToForm(form, settings) {
  for (const [key, value] of Object.entries(settings)) {
    const el = form.querySelector(`[name="${key}"]`);
    if (el) {
      el.value = value ?? "";
    }
  }
}

function updateInspectorStatus(text) {
  const status = document.getElementById("statusText");
  if (status) {
    status.textContent = text;
  }
}

async function loadJson(url) {
  const response = await fetch(url, { method: "GET" });
  if (!response.ok) {
    throw new Error(`GET ${url} failed with ${response.status}`);
  }
  return await response.json();
}
