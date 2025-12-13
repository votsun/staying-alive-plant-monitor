import express from "express";
import { WebSocketServer } from "ws";
import { EventHubConsumerClient } from "@azure/event-hubs";

const IOTHUB_EVENTHUB_CONNECTION = process.env.IOTHUB_EVENTHUB_CONNECTION; // Event Hub-compatible conn string
const PORT = process.env.PORT || 3000;

if (!IOTHUB_EVENTHUB_CONNECTION) {
  console.error("Missing env var IOTHUB_EVENTHUB_CONNECTION");
  process.exit(1);
}

const app = express();
app.use(express.static("public"));

const server = app.listen(PORT, () => {
  console.log(`Open: http://localhost:${PORT}`);
});

const wss = new WebSocketServer({ server });

function broadcast(obj) {
  const msg = JSON.stringify(obj);
  for (const client of wss.clients) {
    if (client.readyState === 1) client.send(msg);
  }
}

// Parse either your text/plain payload or JSON payload
function normalize(body) {
  const s = Buffer.isBuffer(body) ? body.toString("utf8") : String(body);

  // Try JSON first
  try {
    const j = JSON.parse(s);
    return {
      soil_moisture_pct: j.soil_moisture_pct ?? null,
      soil_raw: j.soil_raw ?? null,
      is_dry: !!j.is_dry,
      message: j.status_message ?? null,
      timestamp: new Date().toISOString()
    };
  } catch {}

  // Fallback: parse your current plain text format
  const mMoist = s.match(/moisture=([0-9]+)%/);
  const mRaw = s.match(/raw=([0-9]+)/);
  const isDry = s.includes("[ALERT]");
  return {
    soil_moisture_pct: mMoist ? Number(mMoist[1]) : null,
    soil_raw: mRaw ? Number(mRaw[1]) : null,
    is_dry: isDry,
    message: s.trim(),
    timestamp: new Date().toISOString()
  };
}

(async () => {
  const consumer = new EventHubConsumerClient("$Default", IOTHUB_EVENTHUB_CONNECTION);

  console.log("Listening to IoT Hub events...");
  consumer.subscribe({
    processEvents: async (events) => {
      for (const ev of events) {
        // IoT Hub message body is usually here:
        const normalized = normalize(ev.body);
        broadcast(normalized);
        console.log(normalized.message ?? normalized);
      }
    },
    processError: async (err) => console.error("EventHub error:", err)
  });
})();