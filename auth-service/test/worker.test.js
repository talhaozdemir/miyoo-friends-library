import {test} from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFileSync} from 'node:fs';
import {createHandler,digest} from '../src/worker.js';

// Real SQLite statements behind the small D1 API surface. No Cloudflare account,
// real Google token, or upstream network is used by this suite.
function database(){
  const db=new DatabaseSync(':memory:');db.exec(readFileSync(new URL('../migrations/0001_pairing.sql',import.meta.url),'utf8'));
  function prepared(sql,values=[]){return {
    bind(...v){return prepared(sql,v);},
    async first(){return db.prepare(sql).get(...values)||null;},
    async run(){const v=db.prepare(sql).run(...values);return {success:true,meta:{changes:Number(v.changes)}};}
  };}
  return {raw:db,prepare:prepared,async batch(statements){db.exec('BEGIN');try{const results=[];for(const s of statements)results.push(await s.run());db.exec('COMMIT');return results;}catch(e){db.exec('ROLLBACK');throw e;}}};
}
function fixture(mode='ok'){
  const db=database();let now=1800000000000;const calls=[];
  const env={DB:db,PUBLIC_ORIGIN:'https://miyoo-auth.example.workers.dev',ROOT_FOLDER_ID:'root_fixture',GOOGLE_CLIENT_ID:'fixture.apps.googleusercontent.com',GOOGLE_CLIENT_SECRET:'MOCK_PRIVATE_WEB_SECRET',SESSION_KEY:Buffer.alloc(32,42).toString('base64url')};
  const mock=async(input,init)=>{
    const url=String(input);calls.push({url,init});
    if(url==='https://oauth2.googleapis.com/token'){
      const body=new URLSearchParams(init.body);assert.equal(body.get('client_secret'),env.GOOGLE_CLIENT_SECRET);
      if(body.get('grant_type')==='authorization_code'){
        assert.equal(body.get('code'),'MOCK_CODE');assert.match(body.get('code_verifier'),/^[A-Za-z0-9_-]{43}$/);
        const token={access_token:'MOCK_ACCESS',refresh_token:'MOCK_REFRESH',token_type:'Bearer',expires_in:3600,scope:'https://www.googleapis.com/auth/drive.readonly'};
        if(mode==='no_refresh')delete token.refresh_token;
        if(mode==='wide_scope')token.scope+=' https://www.googleapis.com/auth/drive';
        return Response.json(token);
      }
      assert.equal(body.get('grant_type'),'refresh_token');assert.equal(body.get('refresh_token'),'MOCK_REFRESH');
      if(mode==='revoked')return Response.json({error:'invalid_grant',error_description:'MOCK_REFRESH DO_NOT_LOG'},{status:400});
      if(mode==='offline')throw Error('DO_NOT_LOG MOCK_REFRESH');
      return Response.json({access_token:'MOCK_RENEWED_ACCESS',token_type:'Bearer',expires_in:3600});
    }
    assert.equal(url,'https://www.googleapis.com/drive/v3/files/root_fixture?fields=id%2CmimeType&supportsAllDrives=true');
    assert.equal(init.headers.Authorization,'Bearer MOCK_ACCESS');
    return Response.json(mode==='folder_denied'?{error:'not_found'}:{id:'root_fixture',mimeType:'application/vnd.google-apps.folder'},{status:mode==='folder_denied'?404:200});
  };
  const worker=createHandler({fetchImpl:mock,clock:()=>now});
  const request=(path,body,headers={})=>worker.fetch(new Request(env.PUBLIC_ORIGIN+path,body===undefined?{headers}:{method:'POST',headers:{'Content-Type':'application/json',...headers},body:JSON.stringify(body)}),env);
  const secret='a'.repeat(43);
  async function create(){const response=await request('/v1/pair/create',{device_secret:secret,root_folder_id:env.ROOT_FOLDER_ID});assert.equal(response.status,200);return response.json();}
  const device=(p)=>({pairing_id:p.pairing_id,device_secret:secret});
  async function authorize(p){
    const get=await request('/pair/'+p.pairing_id),html=await get.text(),cookie=get.headers.get('Set-Cookie').split(';')[0];
    assert.ok(html.includes(p.display_code));const nonce=html.match(/name="nonce" value="([^"]+)"/)[1];
    const response=await worker.fetch(new Request(env.PUBLIC_ORIGIN+'/pair/'+p.pairing_id+'/authorize',{method:'POST',headers:{Origin:env.PUBLIC_ORIGIN,Cookie:cookie,'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({nonce})}),env);
    assert.equal(response.status,303);const target=new URL(response.headers.get('Location'));
    assert.equal(target.origin,'https://accounts.google.com');assert.equal(target.searchParams.get('code_challenge_method'),'S256');assert.equal(target.searchParams.get('access_type'),'offline');assert.equal(target.searchParams.get('prompt'),'consent');
    return {cookie,target};
  }
  async function callback(a,override={}){return request('/oauth/callback?'+new URLSearchParams({state:a.target.searchParams.get('state'),code:'MOCK_CODE',...override}),undefined,{Cookie:a.cookie});}
  async function ready(){const p=await create(),a=await authorize(p),r=await callback(a);assert.equal(r.status,200);return {p,a};}
  async function refresh(rt='MOCK_REFRESH'){
    return worker.fetch(new Request(env.PUBLIC_ORIGIN+'/v1/token',{method:'POST',headers:{Authorization:'Basic '+btoa(env.GOOGLE_CLIENT_ID+':'),'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({grant_type:'refresh_token',refresh_token:rt})}),env);
  }
  return {db,env,worker,request,create,device,authorize,callback,ready,refresh,calls,advance(seconds){now+=seconds*1000;},close(){db.raw.close();}};
}

test('health contains no account data and configured endpoints reject the wrong origin',async()=>{const f=fixture();try{assert.deepEqual(await(await f.request('/health')).json(),{service:'miyoo-friends-auth',protocol:1});const r=await f.worker.fetch(new Request('https://evil.test/v1/token',{method:'POST',body:'x'}),f.env);assert.equal(r.status,400);}finally{f.close();}});
test('first pairing, PKCE browser flow, explicit device claim, retry and ack',async()=>{const f=fixture();try{
  const {p}=await f.ready();const status=await f.request('/v1/pair/status',f.device(p));assert.deepEqual(await status.json(),{status:'ready',display_code:p.display_code});
  const stored=f.db.raw.prepare('SELECT * FROM pairings').get();assert.ok(stored.token_cipher);assert.ok(!JSON.stringify(stored).includes('MOCK_REFRESH'));assert.ok(!stored.oauth_cipher);
  const claim=await f.request('/v1/pair/claim',f.device(p)),token=await claim.json();assert.equal(token.refresh_token,'MOCK_REFRESH');assert.equal(token.token_url,f.env.PUBLIC_ORIGIN+'/v1/token');assert.equal(token.client_secret,undefined);
  assert.equal(claim.headers.get('Cache-Control'),'no-store, max-age=0');assert.equal((await f.request('/v1/pair/claim',f.device(p))).status,200);
  assert.equal((await f.request('/v1/pair/ack',f.device(p))).status,200);assert.equal(f.db.raw.prepare('SELECT count(*) AS n FROM pairings').get().n,0);
  assert.equal(f.db.raw.prepare('SELECT digest FROM grants').get().digest,await digest('MOCK_REFRESH'));
}finally{f.close();}});
test('pending claims and wrong device proofs cannot obtain credentials',async()=>{const f=fixture();try{const p=await f.create();assert.equal((await f.request('/v1/pair/claim',f.device(p))).status,409);assert.equal((await f.request('/v1/pair/status',{...f.device(p),device_secret:'b'.repeat(43)})).status,404);assert.equal((await f.request('/v1/pair/create',{device_secret:'a',root_folder_id:'root_fixture'})).status,400);assert.equal((await f.request('/v1/pair/create',{device_secret:'a'.repeat(43),root_folder_id:'other_root'})).status,400);}finally{f.close();}});
test('CSRF and callback replay are rejected',async()=>{const f=fixture();try{const p=await f.create();const bad=await f.worker.fetch(new Request(f.env.PUBLIC_ORIGIN+'/pair/'+p.pairing_id+'/authorize',{method:'POST',headers:{Origin:'https://evil.test'},body:'nonce=x'}),f.env);assert.equal(bad.status,403);const a=await f.authorize(p);assert.equal((await f.callback(a,{state:'b'.repeat(43)})).status,400);assert.equal((await f.callback(a)).status,200);assert.equal((await f.callback(a)).status,400);assert.equal(f.calls.filter(x=>x.url==='https://oauth2.googleapis.com/token').length,1);}finally{f.close();}});
test('Google denial, missing refresh permission, excessive scope and wrong account do not create grants',async()=>{for(const mode of ['no_refresh','wide_scope','folder_denied']){const f=fixture(mode);try{const p=await f.create(),a=await f.authorize(p);assert.equal((await f.callback(a)).status,403);const r=await f.request('/v1/pair/status',f.device(p));assert.equal((await r.json()).status,'error');assert.equal(f.db.raw.prepare('SELECT count(*) AS n FROM grants').get().n,0);assert.equal(f.db.raw.prepare('SELECT token_cipher FROM pairings').get().token_cipher,null);}finally{f.close();}}
 const f=fixture();try{const p=await f.create(),a=await f.authorize(p);assert.equal((await f.callback(a,{error:'access_denied'})).status,403);assert.equal(f.calls.length,0);}finally{f.close();}});
test('cancel and expiration prevent token delivery and prune encrypted state',async()=>{const f=fixture();try{const p=await f.create(),a=await f.authorize(p);assert.equal((await f.request('/v1/pair/cancel',f.device(p))).status,200);assert.equal((await f.callback(a)).status,400);const {p:expired}=await f.ready();f.advance(301);assert.equal((await f.request('/v1/pair/claim',f.device(expired))).status,404);await f.worker.scheduled({},f.env);assert.equal(f.db.raw.prepare('SELECT count(*) AS n FROM pairings').get().n,0);}finally{f.close();}});
test('native endpoints reject browser origins; polling is throttled',async()=>{const f=fixture();try{const p=await f.create();assert.equal((await f.request('/v1/pair/status',f.device(p),{Origin:f.env.PUBLIC_ORIGIN})).status,403);assert.equal((await f.request('/v1/pair/status',f.device(p))).status,200);assert.equal((await f.request('/v1/pair/status',f.device(p))).status,429);f.advance(5);assert.equal((await f.request('/v1/pair/status',f.device(p))).status,200);}finally{f.close();}});
test('refresh proxy keeps the Web client secret off the device and accepts only claimed grants',async()=>{const f=fixture();try{assert.equal((await f.refresh()).status,400);const {p}=await f.ready();await f.request('/v1/pair/claim',f.device(p));const response=await f.refresh(),body=await response.text();assert.equal(response.status,200);assert.equal(JSON.parse(body).access_token,'MOCK_RENEWED_ACCESS');assert.ok(!body.includes('MOCK_PRIVATE_WEB_SECRET'));assert.ok(!body.includes('MOCK_REFRESH'));assert.equal((await f.refresh()).status,429);assert.equal((await f.refresh('FORGED')).status,400);}finally{f.close();}});
test('real invalid_grant and temporary network failures are distinguished without credential logs',async()=>{for(const mode of ['revoked','offline']){const f=fixture(mode);try{const {p}=await f.ready();await f.request('/v1/pair/claim',f.device(p));const response=await f.refresh(),body=await response.text();assert.equal(response.status,mode==='revoked'?400:503);assert.equal(JSON.parse(body).error,mode==='revoked'?'invalid_grant':'service_unavailable');assert.ok(!body.includes('MOCK_REFRESH'));assert.equal(f.db.raw.prepare('SELECT revoked FROM grants').get().revoked,mode==='revoked'?1:0);}finally{f.close();}}});
test('ciphertext substitution fails authentication and never exposes partial credentials',async()=>{const f=fixture();try{const {p}=await f.ready();f.db.raw.prepare('UPDATE pairings SET token_cipher=?').run('AAAA.AAAA');const response=await f.request('/v1/pair/claim',f.device(p));assert.equal(response.status,503);assert.ok(!(await response.text()).includes('MOCK_REFRESH'));assert.equal(f.db.raw.prepare('SELECT count(*) AS n FROM grants').get().n,0);}finally{f.close();}});
test('creation budget stays bounded on the Free plan',async()=>{const f=fixture();try{for(let i=0;i<15;i++)await f.create();assert.equal((await f.request('/v1/pair/create',{device_secret:'a'.repeat(43),root_folder_id:'root_fixture'})).status,429);assert.equal(f.db.raw.prepare('SELECT count(*) AS n FROM pairings').get().n,15);}finally{f.close();}});
