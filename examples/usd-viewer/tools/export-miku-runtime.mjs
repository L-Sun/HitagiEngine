// Offline conversion only. Playback/skinning/rendering of the resulting USD is Hitagi's.
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
// Run from the repository root after installing three@0.160 in temp/miku-recording.
const base = 'temp/miku-recording';
const modules = path.resolve(base, 'node_modules/three');
const load = relative => import(pathToFileURL(path.join(modules, relative)).href);
const { MeshBasicMaterial, Vector3, Euler, Quaternion, Matrix4 } = await load('build/three.module.js');
const { MMDParser } = await load('examples/jsm/libs/mmdparser.module.js');
const { MMDLoader } = await load('examples/jsm/loaders/MMDLoader.js');
const { MMDAnimationHelper } = await load('examples/jsm/animation/MMDAnimationHelper.js');
const buffer = path => { const b=fs.readFileSync(path); return b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength); };
const parser = new MMDParser.Parser();
const model = parser.parsePmx(buffer('temp/miku-new/source/Black.pmx'), true);
const originalBodyPositions = model.rigidBodies.map(r=>[...r.position]);
const motion = parser.parseVmd(buffer(`${base}/source/vmds/wavefile_v2.vmd`), true);
const loader = new MMDLoader();
loader.meshBuilder.materialBuilder.build = d => d.materials.map(() => new MeshBasicMaterial());
const mesh = loader.meshBuilder.build(model, '');
const clip = loader.animationBuilder.build(motion, mesh);
const bones = mesh.skeleton.bones;
const sorted = [], seen = new Set();
function visit(i) { if(seen.has(i)) return; const p=bones.indexOf(bones[i].parent); if(p>=0)visit(p); seen.add(i); sorted.push(i); }
bones.forEach((_,i)=>visit(i));
const names = bones.map((_,i)=>`b${i}`);
for(const i of sorted) { const p=bones.indexOf(bones[i].parent); if(p>=0) names[i]=`${names[p]}/b${i}`; }
const remap = new Map(sorted.map((i,j)=>[i,j]));
const rest = sorted.map(i=>bones[i].matrix.clone());
const bind = sorted.map(i=>bones[i].matrixWorld.clone());
// The dynamic PMX bones supply a rig, not motion samples. Keep their rest pose
// in the clip; only Hitagi changes them during playback.
// MMDLoader converts PMX absolute body positions to bone-relative offsets in-place.
// Consume its effective physics descriptors exactly once; do not subtract the
// bone position a second time (the old sphere-chain exporter did this incorrectly).
const rigBodies = mesh.geometry.userData.MMD.rigidBodies;
const rigJoints = mesh.geometry.userData.MMD.constraints;
const dynamic = new Set(rigBodies.filter(r=>r.type!==0).map(r=>r.boneIndex));
const selected = sorted.filter(i=>dynamic.has(i));
const restPose = bones.map(b=>({p:b.position.clone(),q:b.quaternion.clone()}));
const transform = (p,r) => new Matrix4().compose(new Vector3(...p),
 new Quaternion().setFromEuler(new Euler(...r)),new Vector3(1,1,1));
const bodyOffsets = rigBodies.map(r=>transform(r.position,r.rotation));
const bodyBinds = rigBodies.map((r,i)=>r.boneIndex>=0 ?
 bones[r.boneIndex].matrixWorld.clone().multiply(bodyOffsets[i]) : bodyOffsets[i].clone());
for(let i=0;i<bodyBinds.length;i++) {
 const actual=new Vector3().setFromMatrixPosition(bodyBinds[i]);
 if(actual.distanceTo(new Vector3(...originalBodyPositions[i]))>1e-4)
  throw Error(`Body ${i} bind position no longer matches the original PMX world position`);
}
const jointFrames = rigJoints.map(j=>transform(j.position,j.rotation));
const fps=60, begin=420, count=720;
const helper = new MMDAnimationHelper({sync:false, pmxAnimation:true});
helper.add(mesh, {animation:clip, physics:false});
const translations=[], rotations=[];
for(let frame=0;frame<=begin+count;frame++) {
  helper.update(frame===0 ? 0 : 1/fps);
  for(const i of selected) { bones[i].position.copy(restPose[i].p); bones[i].quaternion.copy(restPose[i].q); }
  mesh.updateMatrixWorld(true);
  if(frame<begin) continue;
  translations.push(sorted.map(i=>bones[i].position.toArray()));
  rotations.push(sorted.map(i=>{const q=bones[i].quaternion; return [q.w,q.x,q.y,q.z];}));
}
const n = value => { if(!Number.isFinite(value))throw Error('Nonfinite value'); return String(+value.toFixed(7)); };
const tuple = a => `(${a.map(n).join(',')})`;
const arr = a => `[${a.join(',')}]`;
const matrix = m => tupleMatrix(m.elements);
const tupleMatrix = e => `(${[0,4,8,12].map(i=>tuple(e.slice(i,i+4))).join(',')})`;
const jointNames=arr(sorted.map(i=>JSON.stringify(names[i])));
let text = `#usda 1.0\n(defaultPrim="Miku"\nstartTimeCode=0\nendTimeCode=${count}\ntimeCodesPerSecond=${fps}\nframesPerSecond=${fps}\nupAxis="Y"\nmetersPerUnit=1)\n`;
text += `def SkelRoot "Miku" (prepend apiSchemas=["SkelBindingAPI"]) {\nrel skel:skeleton=</Miku/Skeleton>\nrel skel:animationSource=</Miku/Wavefile>\ndouble3 xformOp:scale=(0.1,0.1,0.1)\nuniform token[] xformOpOrder=["xformOp:scale"]\n`;
text += `def Skeleton "Skeleton" {\nuniform token[] joints=${jointNames}\nuniform matrix4d[] restTransforms=${arr(rest.map(matrix))}\nuniform matrix4d[] bindTransforms=${arr(bind.map(matrix))}\n}\n`;
text += `def SkelAnimation "Wavefile" {\nuniform token[] joints=${jointNames}\nhalf3[] scales=${arr(sorted.map(()=>'(1,1,1)'))}\nfloat3[] translations.timeSamples={\n`;
text += translations.map((a,i)=>`${i}:${arr(a.map(tuple))}`).join(',\n');
text += `\n}\nquatf[] rotations.timeSamples={\n`;
text += rotations.map((a,i)=>`${i}:${arr(a.map(tuple))}`).join(',\n');
text += '\n}\n}\n';
text += 'def Scope "Materials" {\n';
model.materials.forEach((m,i)=> {
 const texture = m.textureIndex >= 0;
 const textureFile = texture ? model.textures[m.textureIndex].replaceAll('\\','/') : '';
 text += `def Material "Material${i}" {\ntoken outputs:surface.connect=</Miku/Materials/Material${i}/Surface.outputs:surface>\ndef Shader "Surface" {\nuniform token info:id="UsdPreviewSurface"\nfloat inputs:metallic=0\nfloat inputs:roughness=0.8\nfloat inputs:opacity=${n(m.diffuse[3])}\nfloat inputs:opacityThreshold=0.5\n`;
 text += texture ? `color3f inputs:diffuseColor.connect=</Miku/Materials/Material${i}/Texture.outputs:rgb>\n` : `color3f inputs:diffuseColor=${tuple(m.diffuse.slice(0,3))}\n`;
 if(texture) text += `float inputs:opacity.connect=</Miku/Materials/Material${i}/Texture.outputs:a>\n`;
 text += 'token outputs:surface\n}\n';
 if(texture) text += `def Shader "Texture" {\nuniform token info:id="UsdUVTexture"\nasset inputs:file=@${textureFile}@\ntoken inputs:sourceColorSpace="sRGB"\nfloat2 inputs:st.connect=</Miku/Materials/Material${i}/UV.outputs:result>\nfloat3 outputs:rgb\nfloat outputs:a\n}\ndef Shader "UV" {\nuniform token info:id="UsdPrimvarReader_float2"\ntoken inputs:varname="st"\nfloat2 outputs:result\n}\n`;
 text += '}\n';
});
text += '}\n';
const geometry=mesh.geometry;
for(let i=0;i<geometry.attributes.uv.count;i++) geometry.attributes.uv.setY(i, 1-geometry.attributes.uv.getY(i));
geometry.groups.forEach((group, material)=> {
 const oldIndices=Array.from(geometry.index.array.slice(group.start,group.start+group.count));
 const vertices=[...new Set(oldIndices)];
 const local=new Map(vertices.map((v,i)=>[v,i]));
 const attribute=(name,size)=>vertices.map(v=>tuple(Array.from(geometry.attributes[name].array.slice(v*size,v*size+size))));
 const indices=vertices.flatMap(v=>Array.from(geometry.attributes.skinIndex.array.slice(v*4,v*4+4)).map(i=>remap.get(i)));
 const weights=vertices.flatMap(v=>Array.from(geometry.attributes.skinWeight.array.slice(v*4,v*4+4)).map(n));
 text += `def Mesh "Part${material}" (prepend apiSchemas=["SkelBindingAPI","MaterialBindingAPI"]) {\nuniform token subdivisionScheme="none"\nbool doubleSided=true\npoint3f[] points=${arr(attribute('position',3))}\nnormal3f[] normals=${arr(attribute('normal',3))} (interpolation="vertex")\ntexCoord2f[] primvars:st=${arr(attribute('uv',2))} (interpolation="vertex")\nint[] faceVertexCounts=${arr(Array(group.count/3).fill(3))}\nint[] faceVertexIndices=${arr(oldIndices.map(i=>local.get(i)))}\nint[] primvars:skel:jointIndices=${arr(indices)} (interpolation="vertex";elementSize=4)\nfloat[] primvars:skel:jointWeights=${arr(weights)} (interpolation="vertex";elementSize=4)\nmatrix4d primvars:skel:geomBindTransform=((1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1))\nrel material:binding=</Miku/Materials/Material${material}>\n}\n`;
});
text+='}\n';
const output='temp/runtime-physics/usd';
fs.mkdirSync(output,{recursive:true});
fs.cpSync('temp/miku-new/source/tex',`${output}/tex`,{recursive:true});
const config = `custom int[] hitagi:rig:bodies=${arr(rigBodies.flatMap(r=>[r.boneIndex<0?-1:remap.get(r.boneIndex),r.type,r.shapeType,1<<r.groupIndex,r.groupTarget]))}\n`
 + `custom float[] hitagi:rig:properties=${arr(rigBodies.flatMap(r=>[r.width,r.height,r.depth,r.weight,r.positionDamping,r.rotationDamping,r.friction,r.restitution]).map(n))}\n`
 + `custom matrix4d[] hitagi:rig:bind=${arr(bodyBinds.map(matrix))}\n`
 + `custom matrix4d[] hitagi:rig:offsets=${arr(bodyOffsets.map(matrix))}\n`
 + `custom int[] hitagi:rig:joints=${arr(rigJoints.flatMap(j=>[j.rigidBodyIndex1,j.rigidBodyIndex2]))}\n`
 + `custom matrix4d[] hitagi:rig:frames=${arr(jointFrames.map(matrix))}\n`
 + `custom float[] hitagi:rig:limits=${arr(rigJoints.flatMap(j=>[...j.translationLimitation1,...j.translationLimitation2,...j.rotationLimitation1,...j.rotationLimitation2,...j.springPosition,...j.springRotation]).map(n))}\n`;
text=text.replace('def Skeleton "Skeleton" {\n','def Skeleton "Skeleton" {\n'+config);
fs.writeFileSync(`${output}/miku_bullet_physics.usda`,text);
fs.writeFileSync('temp/runtime-physics/rig-export.json',JSON.stringify({physicsBaked:false,blenderUsed:false,
 sourceSeconds:[begin/fps,(begin+count)/fps],joints:bones.length,bodies:rigBodies.length,constraints:rigJoints.length,
 dynamicBones:selected.length,descriptorSource:'three.js r160 MMDLoader effective PMX bodies and constraints',
 bodyBindings:rigBodies.map((r,i)=>({body:i,bone:r.boneIndex,mode:r.type,bind:bodyBinds[i].elements,offset:bodyOffsets[i].elements}))},null,2));
console.log(`Exported ${rigBodies.length} authored bodies and ${rigJoints.length} six-DOF constraints. No physics samples.`);
