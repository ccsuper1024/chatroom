import React, { useState } from 'react';
import { 
  User, 
  Lock, 
  Bell, 
  Shield, 
  Monitor, 
  Camera, 
  Mic, 
  ChevronRight, 
  LogOut, 
  Save,
  Check,
  MessageSquare,
  ArrowLeft
} from 'lucide-react';

export default function Settings({ onLogout, onBack }) {
  const [activeTab, setActiveTab] = useState('profile');
  const [isSaved, setIsSaved] = useState(false);

  // --- Mock State ---
  const [profile, setProfile] = useState({
    username: 'Felix Chen',
    bio: '正在开发酷炫的聊天室项目 🚀',
    email: 'felix@example.com',
    status: 'online',
    avatar: 'https://api.dicebear.com/7.x/avataaars/svg?seed=Felix'
  });

  const [notifications, setNotifications] = useState({
    desktop: true,
    sound: true,
    preview: false
  });

  const handleSave = () => {
    setIsSaved(true);
    setTimeout(() => setIsSaved(false), 3000);
  };

  // --- Components ---

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

  const ToggleItem = ({ title, desc, initial }) => {
    const [enabled, setEnabled] = useState(initial);
    return (
      <div className="flex items-center justify-between p-4 bg-slate-800/20 border border-slate-800 rounded-2xl hover:border-slate-700 transition">
        <div className="max-w-[80%]">
          <h4 className="font-semibold text-slate-200">{title}</h4>
          <p className="text-xs text-slate-500 mt-1">{desc}</p>
        </div>
        <button 
          onClick={() => setEnabled(!enabled)}
          className={`w-12 h-6 rounded-full transition-colors relative ${enabled ? 'bg-blue-600' : 'bg-slate-700'}`}
        >
          <div className={`absolute top-1 w-4 h-4 rounded-full bg-white transition-all ${enabled ? 'left-7' : 'left-1'}`}></div>
        </button>
      </div>
    );
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 p-4 md:p-8 font-sans">
      <div className="max-w-5xl mx-auto flex flex-col md:flex-row gap-8 mt-12 relative">
        
        {/* Back Button */}
        <button 
          onClick={onBack}
          className="absolute -top-12 left-0 flex items-center gap-2 text-slate-400 hover:text-white transition"
        >
          <ArrowLeft size={20} />
          <span>返回聊天</span>
        </button>
        
        {/* --- Left Navigation --- */}
        <div className="w-full md:w-64 space-y-2">
          <div className="px-4 mb-6">
            <h1 className="text-2xl font-bold">设置</h1>
            <p className="text-slate-500 text-sm">管理您的账号和应用偏好</p>
          </div>
          
          <SidebarItem id="profile" icon={<User size={20} />} label="个人资料" />
          <SidebarItem id="account" icon={<Lock size={20} />} label="账号安全" />
          <SidebarItem id="chat" icon={<MessageSquare size={20} />} label="聊天偏好" />
          <SidebarItem id="notifications" icon={<Bell size={20} />} label="消息通知" />
          <SidebarItem id="devices" icon={<Monitor size={20} />} label="音视频设备" />
          <SidebarItem id="privacy" icon={<Shield size={20} />} label="隐私保护" />
          
          <div className="pt-8 border-t border-slate-800 mt-4">
            <button 
              onClick={onLogout}
              className="w-full flex items-center gap-3 px-4 py-3 text-red-400 hover:bg-red-400/10 rounded-xl transition-all"
            >
              <LogOut size={20} />
              <span className="font-medium">退出登录</span>
            </button>
          </div>
        </div>

        {/* --- Right Content Area --- */}
        <div className="flex-1 bg-slate-900/50 backdrop-blur-xl border border-slate-800 rounded-3xl overflow-hidden flex flex-col shadow-2xl">
          <div className="p-8 flex-1">
            {activeTab === 'profile' && (
              <div className="space-y-8 animate-in fade-in slide-in-from-right-4 duration-300">
                <section>
                  <h2 className="text-xl font-bold mb-6">个人资料</h2>
                  <div className="flex flex-col sm:flex-row items-center gap-6 mb-8">
                    <div className="relative group">
                      <img 
                        src={profile.avatar} 
                        alt="Avatar" 
                        className="w-24 h-24 rounded-2xl object-cover ring-4 ring-slate-800 group-hover:ring-blue-600 transition-all cursor-pointer" 
                      />
                      <div className="absolute inset-0 bg-black/40 rounded-2xl flex items-center justify-center opacity-0 group-hover:opacity-100 transition-all cursor-pointer">
                        <Camera size={24} className="text-white" />
                      </div>
                    </div>
                    <div>
                      <h3 className="font-bold text-lg">{profile.username}</h3>
                      <p className="text-slate-500 text-sm mb-3">建议尺寸 256x256 px, 最大 2MB</p>
                      <button className="px-4 py-1.5 bg-slate-800 hover:bg-slate-700 rounded-lg text-sm font-medium transition">更换头像</button>
                    </div>
                  </div>

                  <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                    <div className="space-y-2">
                      <label className="text-sm font-semibold text-slate-400 ml-1">昵称</label>
                      <input 
                        type="text" 
                        value={profile.username}
                        onChange={(e) => setProfile({...profile, username: e.target.value})}
                        className="w-full bg-slate-800/50 border border-slate-700 rounded-xl px-4 py-2.5 focus:outline-none focus:border-blue-500 transition"
                      />
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-semibold text-slate-400 ml-1">电子邮箱</label>
                      <input 
                        type="email" 
                        disabled
                        value={profile.email}
                        className="w-full bg-slate-900 border border-slate-800 rounded-xl px-4 py-2.5 text-slate-500 cursor-not-allowed"
                      />
                    </div>
                    <div className="md:col-span-2 space-y-2">
                      <label className="text-sm font-semibold text-slate-400 ml-1">个性签名</label>
                      <textarea 
                        rows="3"
                        value={profile.bio}
                        onChange={(e) => setProfile({...profile, bio: e.target.value})}
                        className="w-full bg-slate-800/50 border border-slate-700 rounded-xl px-4 py-2.5 focus:outline-none focus:border-blue-500 transition resize-none"
                      ></textarea>
                    </div>
                  </div>
                </section>
              </div>
            )}

            {activeTab === 'devices' && (
              <div className="space-y-8 animate-in fade-in slide-in-from-right-4 duration-300">
                <section>
                  <h2 className="text-xl font-bold mb-2">音视频设备</h2>
                  <p className="text-slate-400 text-sm mb-8">在这里配置并测试您的视频和音频输入输出设备</p>

                  <div className="space-y-6">
                    {/* Camera Test */}
                    <div className="bg-slate-800/30 border border-slate-800 rounded-2xl p-6">
                      <div className="flex items-center gap-3 mb-4">
                        <Camera size={20} className="text-blue-500" />
                        <span className="font-semibold">摄像头检测</span>
                      </div>
                      <div className="aspect-video bg-black rounded-xl mb-4 flex items-center justify-center text-slate-600 relative overflow-hidden">
                         <div className="absolute inset-0 bg-slate-800 flex flex-col items-center justify-center">
                            <Camera size={48} className="mb-2 opacity-20" />
                            <p className="text-sm">摄像头已关闭</p>
                         </div>
                      </div>
                      <select className="w-full bg-slate-800 border border-slate-700 rounded-xl px-4 py-2 text-sm focus:outline-none">
                        <option>FaceTime HD Camera (Built-in)</option>
                        <option>Logitech C920 External</option>
                      </select>
                    </div>

                    {/* Mic Test */}
                    <div className="bg-slate-800/30 border border-slate-800 rounded-2xl p-6">
                      <div className="flex items-center gap-3 mb-4">
                        <Mic size={20} className="text-green-500" />
                        <span className="font-semibold">麦克风检测</span>
                      </div>
                      <div className="flex items-center gap-4 mb-4">
                        <div className="flex-1 h-3 bg-slate-800 rounded-full overflow-hidden">
                           <div className="w-[45%] h-full bg-gradient-to-r from-green-500 to-yellow-500"></div>
                        </div>
                        <span className="text-xs font-mono text-slate-400">-12 dB</span>
                      </div>
                      <select className="w-full bg-slate-800 border border-slate-700 rounded-xl px-4 py-2 text-sm focus:outline-none">
                        <option>MacBook Pro Microphone (System Default)</option>
                        <option>Yeti Nano USB Microphone</option>
                      </select>
                    </div>
                  </div>
                </section>
              </div>
            )}

            {activeTab === 'chat' && (
              <div className="space-y-8 animate-in fade-in slide-in-from-right-4 duration-300">
                <section>
                  <h2 className="text-xl font-bold mb-6">聊天偏好</h2>
                  <div className="space-y-4">
                    <ToggleItem 
                      title="回车键发送消息" 
                      desc="开启后，按下 Enter 键将直接发送消息，Shift + Enter 换行" 
                      initial={true} 
                    />
                    <ToggleItem 
                      title="消息阅读回执" 
                      desc="允许他人看到您是否已读其消息" 
                      initial={true} 
                    />
                    <ToggleItem 
                      title="拼写检查" 
                      desc="在输入时自动检测并标记错误单词" 
                      initial={false} 
                    />
                  </div>
                </section>
              </div>
            )}
          </div>

          {/* Bottom Action Bar */}
          <div className="p-6 border-t border-slate-800 bg-slate-900/80 flex items-center justify-between">
            <p className="text-sm text-slate-500">最后更新于: 2024-05-20 14:30</p>
            <div className="flex gap-3">
              <button className="px-6 py-2 rounded-xl text-slate-400 hover:text-white transition font-medium">重置</button>
              <button 
                onClick={handleSave}
                className={`px-8 py-2 rounded-xl font-bold flex items-center gap-2 transition-all transform active:scale-95 ${
                  isSaved ? 'bg-green-600 text-white' : 'bg-blue-600 hover:bg-blue-500 text-white shadow-lg shadow-blue-900/30'
                }`}
              >
                {isSaved ? <><Check size={18} /> 已保存</> : <><Save size={18} /> 保存更改</>}
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
