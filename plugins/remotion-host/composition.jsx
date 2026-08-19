import React from "react";
import { Composition } from "remotion";

function EdwardAnimation({ title = "代码动画", x = 0, y = 0, opacity = 1, borderWidth = 0, fontSize = 32 }) {
  return <div style={{ flex: 1, background: "transparent", display: "flex", alignItems: "center", justifyContent: "center" }}>
    <div style={{ color: "#f7ffff", background: "#08bdcc", borderRadius: 14, padding: "16px 44px", fontSize,
      opacity, border: borderWidth > 0 ? `${borderWidth}px solid #f7ffff` : "none", transform: `translate(${-x}px, ${-y}px)` }}>{title}</div>
  </div>;
}

export const RemotionRoot = () => <Composition id="EdwardAnimation" component={EdwardAnimation} durationInFrames={1} fps={30} width={1280} height={720} />;
