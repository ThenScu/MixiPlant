import path from 'node:path';
import fs from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
const skillDir='C:/Users/ADMIN/.codex/plugins/cache/openai-primary-runtime/presentations/26.1112.11217/skills/presentations';
const { finalizePresentation } = await import(pathToFileURL(path.join(skillDir,'container_tools/artifact_tool_utils.mjs')).href);
const workspaceDir='D:/code/Nam4_HK1/IOT/MixiPlant';
const stagingDir=path.join(workspaceDir,'.codex-finalizer');
const finalPath=path.join(workspaceDir,'output','MixiPlan_GopNhom_da_chinh_sua_final.pptx');
await fs.mkdir(stagingDir,{recursive:true});
await fs.mkdir(path.dirname(finalPath),{recursive:true});
const result=await finalizePresentation({
  workspaceDir,
  candidatePath:path.join(workspaceDir,'.build/MixiPlan_GopNhom_da_chinh_sua.pptx'),
  finalPath,
  pythonExecutable:'C:/Program Files/WindowsApps/OpenAI.CodexPrimaryRuntime.v26-1112-737-0_26.1112.737.0_x64__3k8sg7r9htsxt/dependencies/python/python.exe',
  integrityValidatorPath:path.join(skillDir,'container_tools/inspect_presentation_package_integrity.py'),
  layoutValidatorPath:path.join(skillDir,'container_tools/inspect_presentation_layout_geometry.py'),
  layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-bullet-geometry','--validate-heading-fit'],
  requiredNativeTableOwnerSlides:[],
  fontPolicy:{basis:'reference',families:['Segoe UI','Consolas'],referencePath:'C:/Users/ADMIN/Downloads/MixiPlan_BaoCao_DoAn_IoT.pptx',referenceSha256:'8c2574856265033b8e0e422272dfE24f86455df9f0a6339a7847b21a3f841389'.toLowerCase()},
  verifyArtifactToolImport:true,
  receiptPath:path.join(stagingDir,'MixiPlan_GopNhom_da_chinh_sua_final.validation.json')
});
console.log(JSON.stringify(result,null,2));
