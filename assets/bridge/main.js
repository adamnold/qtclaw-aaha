// Compatibility with OpenClaw's notification wire contract. No platform spoofing.
const origin = location.origin;
const publish = snapshot => {
  if (!snapshot || !['granted', 'denied', 'notDetermined'].includes(snapshot.permission)) return;
  window.__OPENCLAW_NATIVE_NOTIFICATIONS__ = snapshot;
  window.dispatchEvent(new CustomEvent('openclaw:native-notifications-status', {detail: snapshot}));
};
publish({permission: 'notDetermined', test: null});
window.addEventListener('message', event => {
  if (event.source !== window || event.origin !== origin || event.data?.qtclaw !== 'snapshot') return;
  publish(event.data.value);
});
const webkit = window.webkit || (window.webkit = {});
const handlers = webkit.messageHandlers || (webkit.messageHandlers = {});
Object.defineProperty(handlers, 'openclawNotifications', {value: Object.freeze({
  postMessage(message) {
    let json;
    try { json = JSON.stringify(message); } catch { return; }
    if (typeof json === 'string' && json.length <= 4096)
      window.postMessage({qtclaw: 'notification', json}, origin);
  }
}), configurable: false, writable: false});
