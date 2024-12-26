import ReactDOM from 'react-dom/client';
import Publisher from './Publisher';
import React, { useState, createElement, useEffect } from 'react';
import { getInterface, CSEvent, isCEP } from './CEP';
import { Settings, SWFType } from './PublisherSettings';
import Locale from './Localization';
import { PublisherContextProvider } from './Context';

export const loadFont = async (callback: () => void) => {
  const font = new FontFace("PublisherFont", `url(${require("./fonts/" + Locale.code)})`, {
    style: "normal",
  });
  await font.load();
  if (font.status == "loaded") {
    document.fonts.add(font);
  }
  callback();
};

function App() {
  const [publisherStateData, setPublisherStateData] = useState<string | undefined>(undefined);
  const [isFontLoaded, setIsFontLoaded] = useState(false);

  useEffect(() => {
    // Font

    loadFont(
      () => {
        setIsFontLoaded(true)
      }
    );

    // Publisher Data
    if (!isCEP()) {
      setPublisherStateData("{}");
      return;
    }

    const CSInterface = getInterface();
    const getData = async () => {
      const publisherData = new Promise((resolve) => {
        CSInterface.addEventListener("com.adobe.events.flash.extension.setstate", function (event: CSEvent) {

          if (event.data === undefined) {
            setPublisherStateData("{}");
          } else {
            setPublisherStateData(event.data);
          }

          resolve(undefined);
        });
      });

      const event = new CSEvent(
        "com.adobe.events.flash.extensionLoaded",
        "APPLICATION",
        undefined,
        "org.scWorkshop.SupercellSWF.PublishSettings"
      );
      event.data = "SupercellSWF publisher loaded";
      CSInterface.dispatchEvent(event);

      await publisherData;
    };
    getData();
  }, []);

  if (publisherStateData !== undefined && isFontLoaded) {
    Settings.restore(publisherStateData);

    return createElement(
      "body",
      {
        key: "publisher_body",
        style: {
          fontFamily: "PublisherFont",
          backgroundColor: `#333333`,
          position: "relative"
        }
      },
      <PublisherContextProvider>
        <Publisher></Publisher>
      </PublisherContextProvider>
    );
  } else {
    return null;
  }
}

const root = ReactDOM.createRoot(
  document.getElementById('root') as HTMLElement
);

root.render(
  <React.StrictMode>
    <App></App>
  </React.StrictMode>
)
