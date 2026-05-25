const conversationList = document.getElementById('conversationList');
const conversationSearch = document.getElementById('conversationSearch');
const conversationCount = document.getElementById('conversationCount');
const chatStream = document.getElementById('chatStream');
const composer = document.getElementById('composer');
const messageInput = document.getElementById('messageInput');
const quickReplies = document.getElementById('quickReplies');
const activeConversationTitle = document.getElementById('activeConversationTitle');
const activeConversationMeta = document.getElementById('activeConversationMeta');
const activeAvatar = document.getElementById('activeAvatar');
const statusText = document.getElementById('statusText');
const infoAvatar = document.getElementById('infoAvatar');
const infoName = document.getElementById('infoName');
const infoBio = document.getElementById('infoBio');
const infoStatus = document.getElementById('infoStatus');
const infoRoom = document.getElementById('infoRoom');
const infoTime = document.getElementById('infoTime');
const memberList = document.getElementById('memberList');

// WebSocket bridge 配置 (参见 frontend/BRIDGE_README.md)
const WS_URL = 'ws://127.0.0.1:8765';
let ws = null;

function connectWs() {
  try {
    ws = new WebSocket(WS_URL);
  } catch (e) {
    console.warn('WebSocket init failed', e);
    statusText.textContent = '桥接初始化失败';
    return;
  }

  ws.addEventListener('open', () => {
    statusText.textContent = `已连接桥接 ${WS_URL}`;
    const roomId = parseInt((activeConversation.room || '').replace('#', ''), 10) || 0;
    ws.send(JSON.stringify({ type: 'join', room: roomId, username: 'lin' }));
  });

  ws.addEventListener('close', () => {
    statusText.textContent = '已断开桥接';
    // 自动重连
    setTimeout(connectWs, 1500);
  });

  ws.addEventListener('error', (ev) => {
    console.warn('ws error', ev);
    statusText.textContent = '桥接错误';
  });

  ws.addEventListener('message', (evt) => {
    try {
      const obj = JSON.parse(evt.data);
      if (obj.type === 'frame' && obj.body) {
        handleIncomingFrame(obj.body);
      }
    } catch (e) {
      console.warn('Invalid WS message', e);
    }
  });
}

function sendMsgToBridge(text) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    appendMessage('system', 'SYS', '未连接到后端桥接，消息仅本地显示', 'incoming');
    return false;
  }
  ws.send(JSON.stringify({ type: 'msg', text }));
  return true;
}

function handleIncomingFrame(body) {
  if (typeof body !== 'string') return;
  if (body.startsWith('CHAT|')) {
    const parts = body.split('|');
    // CHAT|1|MSG|text...
    if (parts[2] === 'MSG') {
      const text = parts.slice(3).join('|');
      appendMessage('peer', 'SV', text, 'incoming');
      return;
    }
    if (parts[2] === 'JOIN') {
      appendMessage('server', 'SV', `用户加入: ${parts.slice(3).join('|')}`, 'incoming');
      return;
    }
    if (parts[2] === 'AUTH_ACK') {
      appendMessage('server', 'SV', `认证成功: ${parts.slice(3).join('|')}`, 'incoming');
      return;
    }
  }
  // 否则当作普通文本展示
  appendMessage('server', 'SV', body, 'incoming');
}

const conversations = [
  {
    id: 'alice',
    room: '#100',
    name: 'Alice',
    initials: 'AL',
    subtitle: '产品经理 · 18 人群聊',
    time: '18:24',
    unread: 7,
    online: '在线',
    status: '正在输入',
    bio: '产品经理，习惯把复杂的协作拆成清晰的聊天流。',
    avatarTone: 'primary',
    active: true,
    quickReplies: ['我先整理一个短版需求。', '这版层级已经很接近成品了。', '可以把消息区再加宽一点。'],
    members: [
      { name: 'bob', initials: 'BO', meta: '设计师' },
      { name: 'chen', initials: 'CH', meta: '前端' },
      { name: 'mila', initials: 'MI', meta: '测试' },
    ],
    messages: [
      { author: 'Alice', initials: 'AL', body: '这个页面要像真正的聊天软件，而不是控制台。', mode: 'incoming', time: '18:05' },
      { author: 'lin', initials: 'UI', body: '已经切成三栏结构，左边会话，中间消息，右边资料。', mode: 'outgoing', time: '18:08' },
      { author: 'Alice', initials: 'AL', body: '很好，再加上气泡消息和头像，体验会更像客户端。', mode: 'incoming', time: '18:24' },
    ],
  },
  {
    id: 'design',
    room: '#220',
    name: 'Design Team',
    initials: 'DE',
    subtitle: '设计评审 · 9 条未读',
    time: '17:42',
    unread: 3,
    online: '忙碌',
    status: '离开 5 分钟',
    bio: '聚焦视觉一致性、信息密度和组件节奏。',
    avatarTone: 'accent',
    quickReplies: ['给我一版浅色主题对照。', '头像可以再做圆一点。', '输入区的边框再收一点。'],
    members: [
      { name: 'iris', initials: 'IR', meta: 'UI' },
      { name: 'ken', initials: 'KE', meta: 'UX' },
      { name: 'maya', initials: 'MA', meta: 'PM' },
    ],
    messages: [
      { author: 'Design Team', initials: 'DE', body: '先做一个很像聊天软件的空间感，消息要一眼能扫过去。', mode: 'incoming', time: '17:18' },
      { author: 'lin', initials: 'UI', body: '我会保留桌面端布局，同时让移动端自动收成单栏。', mode: 'outgoing', time: '17:26' },
    ],
  },
  {
    id: 'ops',
    room: '#480',
    name: 'Ops Center',
    initials: 'OP',
    subtitle: '运维观察台 · 6 人在线',
    time: '16:10',
    unread: 0,
    online: '在线',
    status: '稳定运行',
    bio: '用于查看房间状态、服务监控和消息延迟。',
    avatarTone: 'neutral',
    quickReplies: ['这个房间适合放监控。', '把状态灯做亮一点。', '消息延迟要显眼。'],
    members: [
      { name: 'oliver', initials: 'OL', meta: 'SRE' },
      { name: 'pat', initials: 'PA', meta: '后端' },
      { name: 'quinn', initials: 'QU', meta: '运维' },
    ],
    messages: [
      { author: 'Ops Center', initials: 'OP', body: '健康检查正常，聊天会话延迟也很低。', mode: 'incoming', time: '15:58' },
      { author: 'lin', initials: 'UI', body: '这类状态信息可以在右侧资料栏里固定展示。', mode: 'outgoing', time: '16:02' },
    ],
  },
];

let activeConversation = conversations.find((conversation) => conversation.active) || conversations[0];

function nowClock() {
  const now = new Date();
  return now.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' });
}

function renderConversationList(filterText = '') {
  const query = filterText.trim().toLowerCase();
  const visibleConversations = conversations.filter((conversation) => {
    if (!query) {
      return true;
    }

    return [conversation.name, conversation.room, conversation.subtitle].some((text) => text.toLowerCase().includes(query));
  });

  conversationCount.textContent = String(visibleConversations.length);
  conversationList.innerHTML = visibleConversations.map((conversation) => `
    <button class="conversation-card ${conversation.id === activeConversation.id ? 'active' : ''}" data-id="${conversation.id}">
      <div class="avatar ${conversation.avatarTone === 'accent' ? 'accent' : ''}">${conversation.initials}</div>
      <div>
        <strong>${conversation.name}</strong>
        <p>${conversation.subtitle}</p>
      </div>
      <div class="conversation-meta">
        <span>${conversation.time}</span>
        ${conversation.unread > 0 ? `<span class="unread-badge">${conversation.unread}</span>` : '<span class="unread-badge ghost"> </span>'}
      </div>
    </button>
  `).join('');
}

function renderMessages(messages) {
  chatStream.innerHTML = messages.map((message) => `
    <article class="message ${message.mode}">
      <div class="avatar ${message.mode === 'outgoing' ? 'accent' : ''}">${message.initials}</div>
      <div class="chat-bubble">
        <div class="meta">
          <strong>${message.author}</strong>
          <span>${message.time}</span>
        </div>
        <p>${message.body}</p>
      </div>
    </article>
  `).join('');
  chatStream.scrollTop = chatStream.scrollHeight;
}

function renderQuickReplies(items) {
  quickReplies.innerHTML = items.map((text) => `<button class="reply-chip" type="button">${text}</button>`).join('');
}

function renderMembers(members) {
  memberList.innerHTML = members.map((member) => `
    <div class="mini-member">
      <div class="member-avatar">${member.initials}</div>
      <div>
        <strong class="member-name">${member.name}</strong>
        <span class="member-meta">${member.meta}</span>
      </div>
    </div>
  `).join('');
}

function syncSidebar(conversation) {
  activeConversationTitle.textContent = conversation.name;
  activeConversationMeta.textContent = `${conversation.subtitle} · ${conversation.time} ${conversation.online}`;
  activeAvatar.textContent = conversation.initials;
  statusText.textContent = `端到端会话 · ${conversation.room}`;
  infoAvatar.textContent = conversation.initials;
  infoName.textContent = conversation.name;
  infoBio.textContent = conversation.bio;
  infoStatus.textContent = conversation.status;
  infoRoom.textContent = conversation.room;
  infoTime.textContent = conversation.time;
  renderQuickReplies(conversation.quickReplies);
  renderMembers(conversation.members);
}

function setActiveConversation(conversationId) {
  const conversation = conversations.find((item) => item.id === conversationId);
  if (!conversation) {
    return;
  }

  activeConversation = conversation;
  renderConversationList(conversationSearch.value);
  syncSidebar(conversation);
  renderMessages(conversation.messages);
}

function appendMessage(author, initials, text, mode = 'incoming') {
  const article = document.createElement('article');
  article.className = `message ${mode}`;
  article.innerHTML = `
    <div class="avatar ${mode === 'outgoing' ? 'accent' : ''}">${initials}</div>
    <div class="chat-bubble">
      <div class="meta">
        <strong>${author}</strong>
        <span>${nowClock()}</span>
      </div>
      <p>${text}</p>
    </div>
  `;

  chatStream.appendChild(article);
  chatStream.scrollTop = chatStream.scrollHeight;
}

conversationList.addEventListener('click', (event) => {
  const button = event.target.closest('.conversation-card');
  if (!button) {
    return;
  }

  setActiveConversation(button.dataset.id);
});

conversationSearch.addEventListener('input', (event) => {
  renderConversationList(event.target.value);
});

quickReplies.addEventListener('click', (event) => {
  const button = event.target.closest('.reply-chip');
  if (!button) {
    return;
  }

  messageInput.value = button.textContent;
  messageInput.focus();
});

composer.addEventListener('submit', (event) => {
  event.preventDefault();
  const value = messageInput.value.trim();
  if (!value) {
    return;
  }

  // 本地乐观渲染
  appendMessage('lin', 'UI', value, 'outgoing');
  sendMsgToBridge(value);
  messageInput.value = '';
  messageInput.focus();
});

document.querySelectorAll('.ghost-action').forEach((button) => {
  button.addEventListener('click', () => {
    messageInput.value = button.textContent;
    messageInput.focus();
  });
});

renderConversationList();
setActiveConversation(activeConversation.id);

// 启动 WS 桥接
connectWs();
