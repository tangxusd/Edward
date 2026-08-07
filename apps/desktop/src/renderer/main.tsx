import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { LibraryPanel } from './LibraryPanel.js';

const root = document.getElementById('root');

if (!root) {
  throw new Error('React root element is missing');
}

createRoot(root).render(
  <StrictMode>
    <main><h1>AI 剪视频工具</h1><LibraryPanel /></main>
  </StrictMode>,
);
