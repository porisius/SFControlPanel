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

/* ===================================================================
   Search field + alphabetical sorting for dropdown lists
   - Appears automatically above every list with more than 15 entries
   - Matches the display name AND the internal descriptor
     (e.g. "constructor" finds "Recipe_ConstructorMk1_C")
   - Multiple terms supported ("frame heavy"), case- and accent-insensitive
   - Enter = pick first match, Esc = clear search
   - Original plugin logic is untouched
   =================================================================== */
(function () {
  const MIN_ENTRIES_FOR_SEARCH = 15;
  const MAX_VISIBLE_ROWS = 10;

  function normalize(text) {
    return (text || "")
      .toLocaleLowerCase()
      .normalize("NFD")
      .replace(/[\u0300-\u036f]/g, "");
  }

  function setupSearch(select) {
    if (!select || select.dataset.scpSearch) return;
    select.dataset.scpSearch = "1";

    const input = document.createElement("input");
    input.type = "search";
    input.placeholder = "Search… (Enter = pick first match)";
    input.autocomplete = "off";
    input.style.marginBottom = "4px";
    input.style.display = "none";

    const info = document.createElement("div");
    info.className = "hint";

    select.parentNode.insertBefore(input, select);
    select.parentNode.insertBefore(info, select.nextSibling);

    // Typing in the search field must not trigger a settings save
    input.addEventListener("change", (e) => e.stopPropagation());

    const entries = () => Array.from(select.options).filter((o) => o.value !== "");

    function sortEntries() {
      const current = entries();
      const sorted = current.slice().sort((a, b) =>
        a.textContent.localeCompare(b.textContent, undefined, { sensitivity: "base" })
      );
      if (sorted.every((o, i) => o === current[i])) return;
      const selected = select.value;
      sorted.forEach((o) => select.appendChild(o));
      select.value = selected;
    }

    function applyFilter() {
      const all = entries();
      input.style.display = all.length > MIN_ENTRIES_FOR_SEARCH ? "" : "none";

      const terms = normalize(input.value).split(/\s+/).filter(Boolean);
      let hits = 0;

      for (const o of all) {
        const haystack = normalize(o.textContent + " " + o.value);
        const match = terms.every((t) => haystack.includes(t));
        o.hidden = !match;
        if (match) hits++;
      }

      if (terms.length) {
        // Show matches as an expanded list, click to select
        select.size = Math.min(Math.max(hits, 2), MAX_VISIBLE_ROWS);
        info.textContent = hits === 1 ? "1 match" : `${hits} matches`;
      } else {
        select.removeAttribute("size");
        info.textContent = "";
      }
    }

    function resetSearch() {
      input.value = "";
      applyFilter();
    }

    input.addEventListener("input", applyFilter);

    input.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        const first = entries().find((o) => !o.hidden);
        if (first) {
          select.value = first.value;
          select.dispatchEvent(new Event("change", { bubbles: true }));
        }
        resetSearch();
      } else if (e.key === "Escape") {
        e.preventDefault();
        resetSearch();
      }
    });

    // Reset the search after a selection by click
    select.addEventListener("change", () => {
      if (input.value) resetSearch();
    });

    // The plugin repopulates the list on every refresh -> re-sort / re-filter
    new MutationObserver(() => {
      sortEntries();
      applyFilter();
    }).observe(select, { childList: true });

    sortEntries();
    applyFilter();
  }

  document.addEventListener("DOMContentLoaded", () => {
    document.querySelectorAll("select").forEach(setupSearch);
  });
})();
