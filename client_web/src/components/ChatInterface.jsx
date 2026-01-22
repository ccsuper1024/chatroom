import React, { useState, useEffect, useRef } from 'react';
import { 
  MessageSquare, Phone, Video, MoreVertical, Mic, Image as ImageIcon, Smile, Send, Search, Users, X, Settings
} from 'lucide-react';

export default function ChatInterface({ username, ws, onLogout, onOpenSettings }) {
  const [activeTab, setActiveTab] = useState('chats');
  const [selectedChatId, setSelectedChatId] = useState('global');
  const [inputText, setInputText] = useState('');
  const [messages, setMessages] = useState([]);
  const messagesEndRef = useRef(null);

  // Auto-scroll to bottom
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  // WebSocket Message Handling
  useEffect(() => {
    if (!ws) return;

    const handleMessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'message') {
           setMessages(prev => [...prev, {
             id: Date.now(),
             senderId: msg.username === username ? 'me' : 'other',
             senderName: msg.username,
             text: msg.content,
             time: new Date().toLocaleTimeString(),
             type: 'text'
           }]);
        }
      } catch (e) {
        console.error("Failed to parse message", e);
      }
    };

    ws.addEventListener('message', handleMessage);
    return () => ws.removeEventListener('message', handleMessage);
  }, [ws, username]);

  const handleSendMessage = (e) => {
    e.preventDefault();
    if (!inputText.trim() || !ws) return;

    const payload = {
      type: 'message',
      content: inputText,
      // Default to global/public room for now
      room_id: selectedChatId === 'global' ? '' : selectedChatId
    };

    ws.send(JSON.stringify(payload));

    // Optimistic update
    setMessages(prev => [...prev, {
      id: Date.now(),
      senderId: 'me',
      senderName: 'Me',
      text: inputText,
      time: new Date().toLocaleTimeString(),
      type: 'text'
    }]);

    setInputText('');
  };

  return (
    <div className="flex h-screen bg-slate-950 text-slate-100 font-sans overflow-hidden">
      {/* --- Sidebar --- */}
      <div className="w-80 bg-slate-900 border-r border-slate-800 flex flex-col">
        {/* Header */}
        <div className="p-4 border-b border-slate-800 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-tr from-blue-600 to-indigo-600 flex items-center justify-center shadow-lg shadow-blue-900/20">
              <MessageSquare size={20} className="text-white" />
            </div>
            <div>
              <h1 className="font-bold text-lg">ChatRoom</h1>
              <div className="flex items-center gap-1.5">
                <span className="w-2 h-2 rounded-full bg-emerald-500 animate-pulse"></span>
                <span className="text-xs text-slate-400">Online</span>
              </div>
            </div>
          </div>
          <button onClick={onOpenSettings} className="p-2 hover:bg-slate-800 rounded-lg transition-colors text-slate-400 hover:text-white">
            <Settings size={20} />
          </button>
        </div>

        {/* Search */}
        <div className="p-4">
          <div className="relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-slate-500" size={18} />
            <input 
              type="text" 
              placeholder="Search..." 
              className="w-full bg-slate-800 text-slate-200 pl-10 pr-4 py-2.5 rounded-xl border border-slate-700 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition-all placeholder:text-slate-500"
            />
          </div>
        </div>

        {/* Chat List */}
        <div className="flex-1 overflow-y-auto px-2 space-y-1 custom-scrollbar">
          <button
            onClick={() => setSelectedChatId('global')}
            className={`w-full p-3 flex items-center gap-3 rounded-xl transition-all ${
              selectedChatId === 'global' ? 'bg-blue-600/10 border border-blue-600/20' : 'hover:bg-slate-800 border border-transparent'
            }`}
          >
            <div className="relative">
               <div className="w-12 h-12 rounded-full bg-slate-700 flex items-center justify-center">
                 <Users size={24} />
               </div>
            </div>
            <div className="flex-1 text-left min-w-0">
              <div className="flex items-center justify-between mb-0.5">
                <span className={`font-medium truncate ${selectedChatId === 'global' ? 'text-blue-400' : 'text-slate-200'}`}>
                  Global Chat
                </span>
              </div>
              <p className="text-sm text-slate-500 truncate">Public room</p>
            </div>
          </button>
        </div>
        
        {/* User Info */}
        <div className="p-4 border-t border-slate-800 bg-slate-900/50">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-full bg-slate-700 flex items-center justify-center">
               <span className="font-bold">{username[0]?.toUpperCase()}</span>
            </div>
            <div className="flex-1 min-w-0">
              <h3 className="font-medium text-sm truncate">{username}</h3>
              <p className="text-xs text-slate-500">Online</p>
            </div>
            <button onClick={onLogout} className="p-2 hover:bg-slate-800 rounded-lg text-slate-400 hover:text-red-400 transition-colors">
              <X size={18} />
            </button>
          </div>
        </div>
      </div>

      {/* --- Main Chat Area --- */}
      <div className="flex-1 flex flex-col min-w-0 bg-slate-950/50">
        {/* Chat Header */}
        <div className="h-18 border-b border-slate-800 p-4 flex items-center justify-between backdrop-blur-md bg-slate-950/80 sticky top-0 z-10">
          <div className="flex items-center gap-4">
            <div className="w-10 h-10 rounded-full bg-slate-800 flex items-center justify-center">
              <Users size={20} className="text-slate-400" />
            </div>
            <div>
              <h2 className="font-bold text-lg">Global Chat</h2>
              <p className="text-xs text-slate-400">Everyone is here</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button className="p-2.5 hover:bg-slate-800 rounded-xl text-slate-400 hover:text-blue-400 transition-colors">
              <Phone size={20} />
            </button>
            <button className="p-2.5 hover:bg-slate-800 rounded-xl text-slate-400 hover:text-blue-400 transition-colors">
              <Video size={20} />
            </button>
            <div className="w-px h-6 bg-slate-800 mx-1"></div>
            <button className="p-2.5 hover:bg-slate-800 rounded-xl text-slate-400 hover:text-white transition-colors">
              <MoreVertical size={20} />
            </button>
          </div>
        </div>

        {/* Messages */}
        <div className="flex-1 overflow-y-auto p-4 space-y-6 custom-scrollbar">
          {messages.map((msg, index) => (
            <div 
              key={index} 
              className={`flex gap-3 max-w-[80%] ${msg.senderId === 'me' ? 'ml-auto flex-row-reverse' : ''}`}
            >
              <div className={`w-8 h-8 rounded-full flex items-center justify-center flex-shrink-0 ${
                msg.senderId === 'me' ? 'bg-blue-600' : 'bg-slate-700'
              }`}>
                <span className="text-xs font-bold text-white">
                  {msg.senderName?.[0]?.toUpperCase() || '?'}
                </span>
              </div>
              
              <div className={`space-y-1 ${msg.senderId === 'me' ? 'items-end' : 'items-start'} flex flex-col`}>
                <div className="flex items-baseline gap-2">
                  <span className="text-xs text-slate-400 font-medium">{msg.senderName}</span>
                  <span className="text-[10px] text-slate-600">{msg.time}</span>
                </div>
                <div className={`p-3 rounded-2xl text-sm leading-relaxed shadow-sm ${
                  msg.senderId === 'me' 
                  ? 'bg-blue-600 text-white rounded-tr-none' 
                  : 'bg-slate-800 text-slate-200 rounded-tl-none border border-slate-700'
                }`}>
                  {msg.text}
                </div>
              </div>
            </div>
          ))}
          <div ref={messagesEndRef} />
        </div>

        {/* Input Area */}
        <div className="p-4 border-t border-slate-800 bg-slate-900/30">
          <form onSubmit={handleSendMessage} className="max-w-4xl mx-auto relative flex items-center gap-2 bg-slate-900 p-2 rounded-2xl border border-slate-800 focus-within:border-blue-500/50 focus-within:ring-2 focus-within:ring-blue-500/10 transition-all shadow-lg">
            <button type="button" className="p-2.5 text-slate-400 hover:text-blue-400 hover:bg-slate-800 rounded-xl transition-all">
              <Smile size={20} />
            </button>
            <button type="button" className="p-2.5 text-slate-400 hover:text-blue-400 hover:bg-slate-800 rounded-xl transition-all">
              <ImageIcon size={20} />
            </button>
            
            <input 
              type="text" 
              value={inputText}
              onChange={(e) => setInputText(e.target.value)}
              placeholder="Type a message..." 
              className="flex-1 bg-transparent border-none focus:ring-0 text-slate-200 placeholder:text-slate-500 h-10 px-2"
            />
            
            {inputText.trim() ? (
              <button type="submit" className="p-2.5 bg-blue-600 hover:bg-blue-500 text-white rounded-xl transition-all shadow-lg shadow-blue-600/20">
                <Send size={18} />
              </button>
            ) : (
              <button type="button" className="p-2.5 text-slate-400 hover:text-blue-400 hover:bg-slate-800 rounded-xl transition-all">
                <Mic size={20} />
              </button>
            )}
          </form>
        </div>
      </div>
    </div>
  );
}
