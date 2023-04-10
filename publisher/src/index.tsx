import ReactDOM from 'react-dom/client';
import Publisher from './Publisher';
import { createElement, useState } from 'react';
import { refreshTheme, themeColor } from './Components/themes';
import { getInterface, isCEP } from './CEP';
import { onLoad } from './Components';

function App() {
  onLoad();
  
  const [bgr, setBgr] = useState(`#${themeColor.toString(16)}`);

  if (isCEP()) {
    const CSInterface = getInterface();
    CSInterface.addEventListener(
      CSInterface.THEME_COLOR_CHANGED_EVENT,
      function () {
        refreshTheme();
        setBgr(`#${themeColor.toString(16)}`);
      }
    );
  }

  return createElement("body",
    {
      style: {
        backgroundColor: bgr
      }
    },
    <Publisher />
  )
}

const root = ReactDOM.createRoot(
  document.getElementById('root') as HTMLElement
);

root.render(
  <App />
);

