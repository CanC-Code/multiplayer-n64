const WebSocket = require("ws");
const { v4: uuidv4 } = require("uuid");

const PORT = process.env.PORT || 8080;
const wss = new WebSocket.Server({ port: PORT, host: "0.0.0.0" });

// Room store: roomCode -> { host: client, players: [], spectators: [] }
let rooms = {};

function generateRoomCode() {
  return Math.random().toString(36).substring(2, 8).toUpperCase();
}

wss.on("connection", (ws) => {
  ws.id = uuidv4();
  ws.role = null;
  ws.roomCode = null;

  ws.on("message", (msg) => {
    try {
      const data = JSON.parse(msg);

      // Host creates a room
      if (data.type === "host") {
        const roomCode = generateRoomCode();
        rooms[roomCode] = { host: ws, players: [], spectators: [] };
        ws.role = "host";
        ws.roomCode = roomCode;
        ws.send(JSON.stringify({ type: "room_created", roomCode }));
      }

      // Player joins a room
      else if (data.type === "join" && data.roomCode) {
        const room = rooms[data.roomCode];
        if (!room) {
          ws.send(JSON.stringify({ type: "error", message: "Room not found" }));
          return;
        }
        ws.role = "player";
        ws.roomCode = data.roomCode;
        room.players.push(ws);
        room.host.send(JSON.stringify({ type: "player_joined", id: ws.id }));
        ws.send(JSON.stringify({ type: "joined", roomCode: data.roomCode }));
      }

      // Spectator joins
      else if (data.type === "spectate" && data.roomCode) {
        const room = rooms[data.roomCode];
        if (!room) {
          ws.send(JSON.stringify({ type: "error", message: "Room not found" }));
          return;
        }
        ws.role = "spectator";
        ws.roomCode = data.roomCode;
        room.spectators.push(ws);
        ws.send(JSON.stringify({ type: "spectating", roomCode: data.roomCode }));
      }

      // Relay input/state
      else if (data.type === "input" && ws.roomCode) {
        const room = rooms[ws.roomCode];
        if (room) {
          if (ws.role === "player") {
            // Player input goes to host
            if (room.host.readyState === WebSocket.OPEN) {
              room.host.send(JSON.stringify({ type: "input", id: ws.id, input: data.input }));
            }
          } else if (ws.role === "host") {
            // Host state goes to players and spectators
            [...room.players, ...room.spectators].forEach((client) => {
              if (client.readyState === WebSocket.OPEN) {
                client.send(JSON.stringify({ type: "state", state: data.state }));
              }
            });
          }
        }
      }
    } catch (err) {
      console.error("Message error:", err);
    }
  });

  ws.on("close", () => {
    if (ws.roomCode && rooms[ws.roomCode]) {
      const room = rooms[ws.roomCode];
      if (ws.role === "host") {
        // Host disconnect closes room
        room.players.forEach((p) => p.close());
        room.spectators.forEach((s) => s.close());
        delete rooms[ws.roomCode];
      } else {
        room.players = room.players.filter((p) => p !== ws);
        room.spectators = room.spectators.filter((s) => s !== ws);
      }
    }
  });
});

console.log(`WebSocket relay running on port ${PORT}`);
