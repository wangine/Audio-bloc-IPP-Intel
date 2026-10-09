"""Make a private MUSIC comparison copy; never connect IPC or start a DAC."""
import argparse,base64,copy,json,pathlib,struct,xml.etree.ElementTree as ET

def decode(module):
    data=base64.b64decode(module['serialized_state'],validate=True)
    magic,length=struct.unpack_from('<II',data)
    if magic!=0x21324356 or len(data)!=length+9:
        raise ValueError('Unsupported native Crossover state')
    return ET.fromstring(data[8:8+length])

def semantics(node):
    return (node.tag,dict(node.attrib),tuple(semantics(child) for child in node))

def create(original,destination):
    if destination.exists():raise ValueError('Preserve existing profile; choose another output path')
    document=json.loads(original.read_text(encoding='utf-8-sig'))
    result=copy.deepcopy(document)
    source=next(m for m in document['modules'] if m['instance_id']=='crossover.main')
    target=next(m for m in result['modules'] if m['instance_id']=='crossover.main')
    tree=decode(source)
    parameters={n.get('id'):n for n in tree if n.tag=='PARAM'}
    values={k:float(n.get('value')) for k,n in parameters.items()}
    if values['cinemaSource']!=0:raise ValueError('This reference helper requires the active MUSIC scene')
    link=int(values['bassLinkMode'])
    changes={}
    if values['bassExtractor']>=2:changes['bassExtractor']='0'
    if link<2 and values['scoopExtractor']>=2:changes['scoopExtractor']='0'
    if not changes:raise ValueError('No active adaptive MUSIC extractor to change')
    scene=tree.find('MUSIC_CROSSOVER_SCENE')
    if scene is None:raise ValueError('Native MUSIC scene is missing')
    for key,value in changes.items():
        parameters[key].set('value',value)
        scene.set(key,value)
    xml=ET.tostring(tree,encoding='utf-8',short_empty_elements=True)
    payload=struct.pack('<II',0x21324356,len(xml))+xml+b'\0'
    checksum=14695981039346656037
    for value in payload:checksum=((checksum^value)*1099511628211)&0xffffffffffffffff
    target['serialized_state']=base64.b64encode(payload).decode('ascii')
    target['state_checksum_fnv1a64']=f'{checksum:016x}'
    # Prove all other controls/scenes/metadata survive exactly in value.
    restored=decode(target)
    for child in restored:
        if child.tag=='PARAM' and child.get('id') in changes:
            child.set('value',parameters[child.get('id')].get('value'))
    before=decode(source)
    for key in changes:
        next(n for n in restored if n.tag=='PARAM' and n.get('id')==key).set('value',
            next(n for n in before if n.tag=='PARAM' and n.get('id')==key).get('value'))
        old=before.find('MUSIC_CROSSOVER_SCENE').get(key)
        if old is None:restored.find('MUSIC_CROSSOVER_SCENE').attrib.pop(key,None)
        else:restored.find('MUSIC_CROSSOVER_SCENE').set(key,old)
    assert semantics(before)==semantics(restored),'Unrelated Crossover state changed'
    for old,new in zip(document['modules'],result['modules']):
        if old['instance_id']!='crossover.main':assert old==new
    assert document.get('stage05_devices')==result.get('stage05_devices')
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8')
    print(json.dumps({'status':'PASS','changedControls':{k:[values[k],float(v)] for k,v in changes.items()},
        'currentScene':'MUSIC','legacyAveragePreserved':True,'otherModulePayloadsIdentical':True,
        'dacConfigurationIdentical':True,'hardwareOpened':False,'profile':str(destination)}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original',type=pathlib.Path);parser.add_argument('destination',type=pathlib.Path)
    args=parser.parse_args();create(args.original,args.destination)
