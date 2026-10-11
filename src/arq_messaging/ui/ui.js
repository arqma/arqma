/* Arqma Messenger UI — talks to local ui bridge (/api/*). */
(() => {
  const $ = (id) => document.getElementById(id);
  const views = {
    inbox: { title: "Inbox", lede: "Sealed messages waiting in your inbox namespace." },
    send: { title: "Send", lede: "Deliver a sealed message through the local Arqma stack." },
    contacts: { title: "Contacts", lede: "Remembered names map to recipient public keys." },
    status: { title: "Status", lede: "Identity and stack health — same dark shell as Arqma GUI." },
  };

  async function api(path, options = {}) {
    const res = await fetch(path, {
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
      ...options,
    });
    const text = await res.text();
    let body = {};
    try {
      body = text ? JSON.parse(text) : {};
    } catch (_) {
      body = { raw: text };
    }
    if (!res.ok) {
      const err = new Error(body.error || body.raw || res.statusText);
      err.payload = body;
      throw err;
    }
    return body;
  }

  function showView(name) {
    document.querySelectorAll(".view").forEach((el) => el.classList.add("hidden"));
    document.querySelectorAll(".nav-item").forEach((el) => {
      el.classList.toggle("active", el.dataset.view === name);
    });
    $(`view-${name}`).classList.remove("hidden");
    $("view-title").textContent = views[name].title;
    $("view-lede").textContent = views[name].lede;
  }

  function setLog(id, value) {
    $(id).textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
  }

  async function refreshIdentity() {
    try {
      const st = await api("/api/status");
      $("identity-short").textContent = st.identity
        ? `${st.identity.slice(0, 12)}…${st.identity.slice(-8)}`
        : "No identity — generate one";
      $("status-identity").textContent = st.identity || "(none — click Generate identity)";
      $("status-stack").textContent =
        `storage: ${st.storage_url || "-"}\nrouter: ${st.router_url || "-"}\ntoken: ${st.has_token ? "set" : "missing"}`;
    } catch (err) {
      $("identity-short").textContent = "Status unavailable";
      setLog("status-log", String(err.message || err));
    }
  }

  async function refreshInbox() {
    try {
      const data = await api("/api/inbox");
      const list = $("inbox-list");
      list.innerHTML = "";
      (data.keys || []).forEach((key) => {
        const li = document.createElement("li");
        const left = document.createElement("div");
        left.textContent = key;
        const right = document.createElement("button");
        right.className = "btn secondary";
        right.textContent = "Open";
        right.addEventListener("click", async () => {
          try {
            const opened = await api("/api/open", {
              method: "POST",
              body: JSON.stringify({ key }),
            });
            setLog("inbox-log", opened.plaintext || opened.output || opened);
          } catch (err) {
            setLog("inbox-log", String(err.message || err));
          }
        });
        li.appendChild(left);
        li.appendChild(right);
        list.appendChild(li);
      });
      if (!(data.keys || []).length) {
        setLog("inbox-log", "Inbox empty.");
      } else {
        setLog("inbox-log", `${data.keys.length} key(s).`);
      }
    } catch (err) {
      setLog("inbox-log", String(err.message || err));
    }
  }

  async function refreshContacts() {
    try {
      const data = await api("/api/contacts");
      const list = $("contacts-list");
      list.innerHTML = "";
      Object.entries(data.contacts || {}).forEach(([name, hex]) => {
        const li = document.createElement("li");
        li.innerHTML = `<strong>${name}</strong><span>${hex}</span>`;
        list.appendChild(li);
      });
      setLog("contacts-log", `${Object.keys(data.contacts || {}).length} contact(s).`);
    } catch (err) {
      setLog("contacts-log", String(err.message || err));
    }
  }

  document.querySelectorAll(".nav-item").forEach((btn) => {
    btn.addEventListener("click", () => {
      showView(btn.dataset.view);
      if (btn.dataset.view === "inbox") refreshInbox();
      if (btn.dataset.view === "contacts") refreshContacts();
      if (btn.dataset.view === "status") refreshIdentity();
    });
  });

  $("btn-gen").addEventListener("click", async () => {
    try {
      const data = await api("/api/gen", { method: "POST", body: "{}" });
      setLog("status-log", `Generated:\n${data.identity}`);
      await refreshIdentity();
      showView("status");
    } catch (err) {
      setLog("status-log", String(err.message || err));
    }
  });

  $("btn-refresh-inbox").addEventListener("click", refreshInbox);
  $("btn-open-all").addEventListener("click", async () => {
    try {
      const data = await api("/api/open", { method: "POST", body: "{}" });
      setLog("inbox-log", data.plaintext || data.output || data);
    } catch (err) {
      setLog("inbox-log", String(err.message || err));
    }
  });

  $("btn-send").addEventListener("click", async () => {
    try {
      const data = await api("/api/send", {
        method: "POST",
        body: JSON.stringify({ to: $("send-to").value.trim(), text: $("send-text").value }),
      });
      setLog("send-log", data.key ? `Stored as ${data.key}` : data.output || data);
      $("send-text").value = "";
    } catch (err) {
      setLog("send-log", String(err.message || err));
    }
  });

  $("btn-refresh-contacts").addEventListener("click", refreshContacts);
  $("btn-save-contact").addEventListener("click", async () => {
    try {
      await api("/api/contacts", {
        method: "POST",
        body: JSON.stringify({
          name: $("contact-name").value.trim(),
          hex: $("contact-hex").value.trim(),
        }),
      });
      $("contact-name").value = "";
      $("contact-hex").value = "";
      await refreshContacts();
    } catch (err) {
      setLog("contacts-log", String(err.message || err));
    }
  });

  $("btn-refresh-status").addEventListener("click", refreshIdentity);

  refreshIdentity();
  refreshInbox();
})();
