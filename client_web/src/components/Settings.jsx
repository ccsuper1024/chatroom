import React, { useState } from 'react';
import { 
  User, 
  Lock, 
  Bell, 
  Shield, 
  Monitor, 
  ChevronRight, 
  LogOut,
  MessageSquare
} from 'lucide-react';

export default function Settings({ onLogout, onBack }) {
  const [activeTab, setActiveTab] = useState('profile');

  // --- Mock State ---
  const [profile] = useState({
    username: 'Felix Chen',
    bio: 'Developing cool chatroom project 🚀',
    email: 'felix@example.com',
    status: 'online',
    avatar: 'https://api.dicebear.com/7.x/avataaars/svg?seed=Felix'
  });

  const SidebarItem = ({ id, icon, label }) => (
    <button
      onClick={() => setActiveTab(id)}
      className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl transition-all ${
        activeTab === id 
        ? 'bg-blue-600 text-white shadow-lg shadow-blue-900/20' 
        : 'text-slate-400 hover:bg-slate-800 hover:text-slate-200'
      }`}
    >
      {icon}
      <span className="font-medium">{label}</span>
      {activeTab === id && <ChevronRight size={16} className="ml-auto" />}
    </button>
  );

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 p-4 md:p-8 font-sans">
      <div className="max-w-5xl mx-auto">
        <button onClick={onBack} className="mb-6 text-slate-400 hover:text-white flex items-center gap-2">
           ← Back to Chat
        </button>

        <div className="flex flex-col md:flex-row gap-8">
          {/* --- Left Navigation --- */}
          <div className="w-full md:w-64 space-y-2">
            <div className="px-4 mb-6">
              <h1 className="text-2xl font-bold">Settings</h1>
              <p className="text-slate-500 text-sm">Manage preferences</p>
            </div>
            
            <SidebarItem id="profile" icon={<User size={20} />} label="Profile" />
            <SidebarItem id="account" icon={<Lock size={20} />} label="Security" />
            <SidebarItem id="chat" icon={<MessageSquare size={20} />} label="Chat" />
            <SidebarItem id="notifications" icon={<Bell size={20} />} label="Notifications" />
            <SidebarItem id="devices" icon={<Monitor size={20} />} label="Devices" />
            <SidebarItem id="privacy" icon={<Shield size={20} />} label="Privacy" />
            
            <div className="pt-8 border-t border-slate-800 mt-4">
              <button 
                onClick={onLogout}
                className="w-full flex items-center gap-3 px-4 py-3 text-red-400 hover:bg-red-400/10 rounded-xl transition-all"
              >
                <LogOut size={20} />
                <span className="font-medium">Logout</span>
              </button>
            </div>
          </div>

          {/* --- Right Content Area --- */}
          <div className="flex-1 bg-slate-900/50 backdrop-blur-xl border border-slate-800 rounded-3xl overflow-hidden flex flex-col shadow-2xl">
            <div className="p-8 flex-1">
              {activeTab === 'profile' && (
                <div className="space-y-8 animate-in fade-in slide-in-from-right-4 duration-300">
                  <section>
                    <h2 className="text-xl font-bold mb-6">Profile</h2>
                    <div className="flex flex-col sm:flex-row items-center gap-6 mb-8">
                      <div className="relative group">
                        <img 
                          src={profile.avatar} 
                          alt="Avatar" 
                          className="w-24 h-24 rounded-2xl object-cover ring-4 ring-slate-800 group-hover:ring-blue-600 transition-all cursor-pointer" 
                        />
                      </div>
                      <div className="flex-1 space-y-4 w-full">
                        <div>
                          <label className="block text-slate-400 text-sm font-medium mb-1">Username</label>
                          <input type="text" defaultValue={profile.username} className="w-full bg-slate-800 border border-slate-700 rounded-lg px-4 py-2" />
                        </div>
                        <div>
                          <label className="block text-slate-400 text-sm font-medium mb-1">Bio</label>
                          <textarea defaultValue={profile.bio} className="w-full bg-slate-800 border border-slate-700 rounded-lg px-4 py-2 h-24" />
                        </div>
                      </div>
                    </div>
                  </section>
                </div>
              )}
              {activeTab !== 'profile' && (
                <div className="flex items-center justify-center h-full text-slate-500">
                  Work in progress...
                </div>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
