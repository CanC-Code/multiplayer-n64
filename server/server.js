// server/server.js
// MultiplayerN64 Node.js WebSocket server
// Runs with: `node server/server.js`

const express = require("express");
const http = require("http");
const WebSocket = require("ws");
const { v4: uuidv4 } = require("uuid");

const PORT = process.env.PORT || 3000;

// Express app (serves as a health check endpoint, not the emulator)
const app = express();
app.get("/", (req, res) => res.send("MultiplayerN64 WebSocket Server is running"));

// HTTP + WebSocket server
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// In-memory room store
// Structure: { roomId: { host: playerId, players: Map<playerId, ws>, spectators: Set<ws> } }
const rooms = {};

function createRoom(ws) {
  const roomId = Math.random().toString(36).substring(2, 8); // 6-char code
  const playerId = uuidv4();

  rooms[roomId] = {
    host: playerId,
    players: new Map([[playerId, ws]]),
    spectators: new Set()
  };

  ws.roomId = roomId;
  ws.playerId = playerId;

  ws.send(JSON.stringify({ type: "room-created", roomId, playerId }));
  console.log(`Room ${roomId} created by host ${playerId}`);
}

function joinRoom(ws, roomId, spectator = false) {
  if (!rooms[roomId]) {
    ws.send(JSON.stringify({ type: "error", message: "Room not found" }));
    return;
  }

  const playerId = uuidv4();
  ws.roomId = roomId;

  if (spectator) {
    rooms[roomId].spectators.add(ws);
    ws.playerId = "spectator:" + playerId;
    ws.send(JSON.stringify({ type: "joined-spectator", roomId }));
    console.log(`Spectator joined room ${roomId}`);
  } else {
    rooms[roomId].players.set(playerId, ws);
    ws.playerId = playerId;
    ws.send(JSON.stringify({ type: "joined", roomId, playerId }));
    console.log(`Player ${playerId} joined room ${roomId}`);
  }
}

function broadcast(roomId, data, excludeId = null) {
  const room = rooms[roomId];
  if (!room) return;

  const message = JSON.stringify(data);

  // Send to all players
  for (const [id, client] of room.players.entries()) {
    if (id !== excludeId && client.readyState === WebSocket.OPEN) {
      client.send(message);
    }
  }

  // Send to spectators
  for (const client of room.spectators) {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message);
    }
  }
}

wss.on("connection", (ws) => {
  console.log("New client connected");

  ws.on("message", (message) => {
    try {
      const data = JSON.parse(message);

      switch (data.type) {
        case "create-room":
          createRoom(ws);
          break;

        case "join-room":
          joinRoom(ws, data.roomId, false);
          break;

        case "spectate-room":
          joinRoom(ws, data.roomId, true);
          break;

        case "input":
          // Input events from a player
          if (ws.roomId) {
            broadcast(ws.roomId, {
              type: "input",
              playerId: ws.playerId,
              input: data.input
            }, ws.playerId);
          }
          break;

        case "state-sync":
          // Host can broadcast full game state
          if (ws.roomId && ws.playerId === rooms[ws.roomId].host) {
            broadcast(ws.roomId, {
              type: "state-sync",
              state: data.state
            }, ws.playerId);
          }
          break;

        default:
          ws.send(JSON.stringify({ type: "error", message: "Unknown message type" }));
      }
    } catch (err) {
      console.error("Invalid message:", message, err);
      ws.send(JSON.stringify({ type: "error", message: "Invalid JSON" }));
    }
  });

  ws.on("close", () => {
    console.log("Client disconnected");
    if (!ws.roomId || !rooms[ws.roomId]) return;

    const room = rooms[ws.roomId];
    if (room.players.has(ws.playerId)) {
      room.players.delete(ws.playerId);
      broadcast(ws.roomId, { type: "player-left", playerId: ws.playerId });
    } else if (room.spectators.has(ws)) {
      room.spectators.delete(ws);
    }

    // Delete room if empty
    if (room.players.size === 0 && room.spectators.size === 0) {
      delete rooms[ws.roomId];
      console.log(`Room ${ws.roomId} deleted`);
    }
  });
});

// Start server
server.listen(PORT, () => {
  console.log(`✅ MultiplayerN64 WebSocket Server running on port ${PORT}`);
});✅
