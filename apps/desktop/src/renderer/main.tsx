import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';

const root = document.getElementById('root');

if (!root) {
  throw new Error('React root element is missing');
}

createRoot(root).render(
  <StrictMode>
    <main>AI 剪视频工具</main>
  </StrictMode>,
);
