import React, { useState } from 'react';
import { User, Lock, Mail, Chrome, Github, ArrowRight, MessageSquare } from 'lucide-react';

export default function Login({ onLogin }) {
  const [isRegister, setIsRegister] = useState(false);
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [email, setEmail] = useState(''); // Added email state
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setLoading(true);

    if (!username.trim() || !password.trim()) {
      setError('Username and password are required');
      setLoading(false);
      return;
    }

    try {
      const endpoint = isRegister ? '/register' : '/login';
      const body = isRegister 
        ? { username, password, email, client_type: 'web' }
        : { username, password, client_type: 'web' };

      const response = await fetch(endpoint, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(body),
      });

      const data = await response.json();

      if (data.success) {
        if (isRegister) {
          setIsRegister(false);
          setError('Registration successful! Please login.');
          setLoading(false);
          return;
        }
        onLogin(username, password);
      } else {
        setError(data.error || (isRegister ? 'Registration failed' : 'Login failed'));
      }
    } catch (err) {
      console.error('Auth error:', err);
      setError('Network error. Please try again.');
    } finally {
      setLoading(false);
    }
  };

  const toggleMode = () => {
    setIsRegister(!isRegister);
    setError('');
    setUsername('');
    setPassword('');
    setEmail('');
  };

  return (
    <div className="min-h-screen bg-slate-950 flex items-center justify-center p-4">
      <div className="relative bg-slate-900 rounded-[30px] shadow-2xl w-full max-w-[900px] min-h-[550px] overflow-hidden">
        
        {/* Sign Up Form Container */}
        <div 
          className={`absolute top-0 left-0 h-full w-1/2 flex flex-col items-center justify-center p-10 transition-all duration-700 ease-in-out z-20 ${
            isRegister 
              ? 'translate-x-full opacity-100 visible' 
              : 'opacity-0 invisible'
          }`}
        >
          <form onSubmit={handleSubmit} className="w-full flex flex-col items-center text-center px-8">
            <h1 className="text-3xl font-bold text-white mb-2">创建账户</h1>
            <p className="text-slate-400 text-sm mb-6">注册以开始使用</p>
            
            {error && !isRegister && <div className="text-red-500 text-sm mb-2">{error}</div>}
            {error && isRegister && <div className="text-red-500 text-sm mb-2">{error}</div>}

            <div className="w-full space-y-4">
              <div className="relative">
                 <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                  <User size={18} className="text-slate-500" />
                 </div>
                 <input 
                  type="text" 
                  placeholder="用户名" 
                  className="w-full bg-slate-800 text-white text-sm rounded-lg py-3 pl-10 pr-4 outline-none focus:ring-2 focus:ring-blue-500 transition-all placeholder-slate-500"
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                />
              </div>
              <div className="relative">
                 <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                  <Mail size={18} className="text-slate-500" />
                 </div>
                 <input 
                  type="email" 
                  placeholder="邮箱" 
                  className="w-full bg-slate-800 text-white text-sm rounded-lg py-3 pl-10 pr-4 outline-none focus:ring-2 focus:ring-blue-500 transition-all placeholder-slate-500"
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                />
              </div>
              <div className="relative">
                 <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                  <Lock size={18} className="text-slate-500" />
                 </div>
                 <input 
                  type="password" 
                  placeholder="密码" 
                  className="w-full bg-slate-800 text-white text-sm rounded-lg py-3 pl-10 pr-4 outline-none focus:ring-2 focus:ring-blue-500 transition-all placeholder-slate-500"
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                />
              </div>
            </div>

            <button 
              type="submit" 
              disabled={loading}
              className="mt-6 w-full bg-blue-600 text-white font-semibold py-3 rounded-lg hover:bg-blue-500 transition-colors shadow-lg shadow-blue-600/30"
            >
              {loading ? '处理中...' : '立即注册'}
            </button>
          </form>
        </div>

        {/* Sign In Form Container */}
        <div 
          className={`absolute top-0 left-0 h-full w-1/2 flex flex-col items-center justify-center p-10 transition-all duration-700 ease-in-out z-20 ${
            isRegister 
              ? 'translate-x-full opacity-0 invisible' 
              : 'translate-x-0 opacity-100 visible'
          }`}
        >
          <form onSubmit={handleSubmit} className="w-full flex flex-col px-8">
            <h1 className="text-3xl font-bold text-white mb-2 text-center">欢迎回来</h1>
            <p className="text-slate-400 text-sm mb-8 text-center">请输入您的账号信息以继续</p>

            {error && !isRegister && <div className="text-red-500 text-sm mb-4 text-center">{error}</div>}

            <div className="space-y-4">
              <div>
                <label className="block text-slate-400 text-xs mb-1 ml-1">用户名 / 邮箱</label>
                <div className="relative">
                  <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                    <Mail size={18} className="text-slate-500" />
                  </div>
                  <input 
                    type="text" 
                    placeholder="name@example.com" 
                    className="w-full bg-slate-800 text-white text-sm rounded-lg py-3 pl-10 pr-4 outline-none focus:ring-2 focus:ring-blue-500 transition-all placeholder-slate-500"
                    value={username}
                    onChange={(e) => setUsername(e.target.value)}
                  />
                </div>
              </div>
              
              <div>
                <label className="block text-slate-400 text-xs mb-1 ml-1">密码</label>
                <div className="relative">
                  <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                    <Lock size={18} className="text-slate-500" />
                  </div>
                  <input 
                    type="password" 
                    placeholder="........" 
                    className="w-full bg-slate-800 text-white text-sm rounded-lg py-3 pl-10 pr-4 outline-none focus:ring-2 focus:ring-blue-500 transition-all placeholder-slate-500"
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                  />
                </div>
              </div>
            </div>

            <div className="flex justify-end mt-2 mb-6">
              <a href="#" className="text-xs text-slate-400 hover:text-white transition-colors">忘记密码？</a>
            </div>

            <button 
              type="submit"
              disabled={loading}
              className="w-full bg-blue-600 text-white font-semibold py-3 rounded-lg hover:bg-blue-500 transition-colors shadow-lg shadow-blue-600/30"
            >
               {loading ? '登录中...' : '立即登录'}
            </button>

            <div className="mt-8 relative flex items-center justify-center">
              <div className="absolute inset-0 flex items-center">
                <div className="w-full border-t border-slate-700"></div>
              </div>
              <div className="relative bg-slate-900 px-2 text-xs text-slate-500 uppercase">Or Continue With</div>
            </div>

            <div className="mt-6 flex justify-center">
              <button type="button" className="flex items-center justify-center w-full border border-slate-700 rounded-lg py-2.5 text-slate-300 hover:bg-slate-800 transition-colors gap-2">
                <Chrome size={20} />
                <span className="text-sm">Google</span>
              </button>
            </div>
          </form>
        </div>

        {/* Overlay Container */}
        <div 
          className={`absolute top-0 left-1/2 w-1/2 h-full overflow-hidden transition-transform duration-700 ease-in-out z-50 ${
            isRegister ? '-translate-x-full' : ''
          }`}
        >
          <div 
            className={`bg-gradient-to-br from-blue-600 to-indigo-700 text-white relative -left-full h-full w-[200%] transform transition-transform duration-700 ease-in-out flex items-center justify-center ${
              isRegister ? 'translate-x-1/2' : 'translate-x-0'
            }`}
          >
            
            {/* Overlay Left (Visible when Registering -> Shows "Welcome Back") */}
            <div className={`w-1/2 h-full flex flex-col items-center justify-center px-12 text-center transform transition-transform duration-700 ease-in-out ${
                isRegister ? 'translate-x-0' : '-translate-x-[20%]'
            }`}>
              <h1 className="text-3xl font-bold mb-6">欢迎回来！</h1>
              <p className="text-sm mb-8 leading-relaxed opacity-90">
                如果您已经拥有账号，请登录以保持与朋友的联系。
              </p>
              <button 
                onClick={toggleMode}
                className="bg-transparent border border-white text-white font-semibold py-3 px-12 rounded-lg hover:bg-white hover:text-blue-600 transition-colors"
              >
                去登录
              </button>
            </div>

            {/* Overlay Right (Visible when Logging in -> Shows "Hello, Friend!") */}
            <div className={`w-1/2 h-full flex flex-col items-center justify-center px-12 text-center transform transition-transform duration-700 ease-in-out ${
                isRegister ? 'translate-x-[20%]' : 'translate-x-0'
            }`}>
              <h1 className="text-3xl font-bold mb-6">你好，朋友！</h1>
              <p className="text-sm mb-8 leading-relaxed opacity-90">
                还没有账号？输入您的个人详细信息，开始与我们的旅程。
              </p>
              <button 
                onClick={toggleMode}
                className="bg-transparent border border-white text-white font-semibold py-3 px-12 rounded-lg hover:bg-white hover:text-blue-600 transition-colors"
              >
                去注册
              </button>
            </div>

          </div>
        </div>

      </div>
    </div>
  );
}
