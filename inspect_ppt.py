import zipfile, re, os, html
import sys
sys.stdout.reconfigure(encoding='utf-8')
files=[r'D:\code\Nam4_HK1\IOT\MixiPlant\output\MixiPlan_GopNhom_da_chinh_sua_final.pptx']
for f in files:
    print('FILE',os.path.basename(f))
    z=zipfile.ZipFile(f)
    names=[n for n in z.namelist() if n.startswith('ppt/slides/slide') and n.endswith('.xml')]
    for n in sorted(names, key=lambda x:int(re.search(r'slide(\d+)',x).group(1))):
        s=z.read(n).decode('utf-8')
        txt=' '.join(re.findall(r'<a:t>(.*?)</a:t>',s))
        print(n, html.unescape(txt)[:700])
