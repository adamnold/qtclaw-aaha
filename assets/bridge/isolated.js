const origin = location.origin;
let native = null;
const queue = [];
window.addEventListener('message', event => {
  if (event.source !== window || event.origin !== origin || event.data?.qtclaw !== 'notification') return;
  const json = event.data.json;
  if (typeof json !== 'string' || json.length > 4096) return;
  if (native) native.postMessage(json);
  else if (queue.length < 20) queue.push(json);
});
new QWebChannel(qt.webChannelTransport, channel => {
  native = channel.objects.notifications;
  native.snapshot.connect(json => {
    try { window.postMessage({qtclaw: 'snapshot', value: JSON.parse(json)}, origin); } catch {}
  });
  for (const json of queue.splice(0)) native.postMessage(json);
  native.postMessage('{"type":"status"}');
});
