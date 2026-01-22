import React, { useState, useEffect } from 'react';
import Login from './components/Login';
import ChatInterface from './components/ChatInterface';
import Settings from './components/Settings';

function App() {
  const [user, setUser] = useState(null);
  const [view, setView] = useState('login'); // login, chat, settings
  const [ws, setWs] = useState(null);

  const handleLogin = (username) => {
    // 1. WebSocket Connect
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host || 'localhost:8080';
    // If running in dev mode (port 5173), we need to connect to backend port 8080
    // But since we are building and serving from backend, window.location.host is correct.
    // However, for development (Vite), we might need to hardcode or proxy.
    // Let's assume production build first.
    // If dev mode, hardcode to 8080.
    const wsUrl = window.location.port === '5173' 
      ? `ws://${window.location.hostname}:8080` 
      : `${protocol}//${host}`;

    const socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      // Send Login Message
      socket.send(JSON.stringify({
        type: 'login',
        username: username
      }));
    };

    socket.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'login_response' && msg.success) {
          setUser(msg.username);
          setView('chat');
        }
      } catch (e) {
        console.error("Login response parse error", e);
      }
    };

    setWs(socket);
  };

  const handleLogout = () => {
    if (ws) ws.close();
    setWs(null);
    setUser(null);
    setView('login');
  };

  if (view === 'login') {
    return <Login onLogin={handleLogin} />;
  }

  if (view === 'settings') {
    return <Settings onLogout={handleLogout} onBack={() => setView('chat')} />;
  }

  return (
    <ChatInterface 
      username={user} 
      ws={ws} 
      onLogout={handleLogout}
      onOpenSettings={() => setView('settings')}
    />
  );
}

export default App;
