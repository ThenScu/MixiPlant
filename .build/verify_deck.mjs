import fs from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
const { FileBlob, PresentationFile } = await import(pathToFileURL('C:/Program Files/WindowsApps/OpenAI.CodexPrimaryRuntime.v26-1112-737-0_26.1112.737.0_x64__3k8sg7r9htsxt/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs').href);
const deck=await PresentationFile.importPptx(await FileBlob.load('D:/code/Nam4_HK1/IOT/MixiPlant/output/MixiPlan_GopNhom_da_chinh_sua_final.pptx'));
console.log('slides',deck.slides.items.length);
console.log((await deck.inspect({kind:'slide,notes',maxChars:12000})).ndjson);
await fs.writeFile('output/montage.webp',new Uint8Array(await deck.render({montage:true}))); 
