import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Miniflare,convertV4MiniflareOptions} from 'miniflare';

// Exercise the deployed default handler in workerd, not a Node fetch substitute.
// Outbound requests are intercepted after workerd constructs them. No real
// Google account, credential, Cloudflare account or external network is used.
const origin='https://mfl-runtime.example.workers.dev';
const client='fixture.apps.googleusercontent.com';
const schema=readFileSync(new URL('../migrations/0001_pairing.sql',import.meta.url),'utf8');
const source=readFileSync(new URL('../src/worker.js',import.meta.url),'utf8');

async function fixture(mode='ok'){
  const calls=[];
  const mf=new Miniflare(convertV4MiniflareOptions({
    modules:true,script:source,compatibilityDate:'2026-11-15',
    compatibilityFlags:['nodejs_compat'],d1Databases:['DB'],
    bindings:{PUBLIC_ORIGIN:origin,GOOGLE_CLIENT_ID:client,ROOT_FOLDER_ID:'root_fixture',
      GOOGLE_CLIENT_SECRET:'MOCK_WEB_SECRET',SESSION_KEY:Buffer.alloc(32,42).toString('base64url')},
    outboundService:async(request)=>{
      const url=new URL(request.url);calls.push(request.url);
      if(url.href==='https://oauth2.googleapis.com/token'){
        const body=new URLSearchParams(await request.text());
        assert.equal(body.get('client_id'),client);
        assert.equal(body.get('client_secret'),'MOCK_WEB_SECRET');
        if(mode==='token_redirect')return new Response(null,{status:307,headers:{Location:'https://forbidden.example/secret'}});
        if(body.get('grant_type')==='refresh_token'){
          assert.equal(body.get('refresh_token'),'MOCK_REFRESH');
          return Response.json({access_token:'MOCK_RENEWED',token_type:'Bearer',expires_in:3600});
        }
        assert.equal(body.get('grant_type'),'authorization_code');
        assert.equal(body.get('code'),'MOCK_CODE');
        assert.match(body.get('code_verifier'),/^[A-Za-z0-9_-]{43}$/);
        return Response.json({access_token:'MOCK_ACCESS',refresh_token:'MOCK_REFRESH',token_type:'Bearer',expires_in:3600,scope:'https://www.googleapis.com/auth/drive.readonly'});
      }
      assert.equal(url.origin,'https://www.googleapis.com');
      assert.equal(url.pathname,'/drive/v3/files/root_fixture');
      assert.equal(request.headers.get('Authorization'),'Bearer MOCK_ACCESS');
      if(mode==='folder_redirect')return new Response(null,{status:302,headers:{Location:'https://forbidden.example/bearer'}});
      return Response.json({id:'root_fixture',mimeType:'application/vnd.google-apps.folder'});
    }
  }));
  try{
    const db=await mf.getD1Database('DB');
    for(const statement of schema.split(';').map(s=>s.trim()).filter(Boolean))await db.prepare(statement).run();
    const request=(path,init)=>mf.dispatchFetch(origin+path,init);
    const post=(path,body)=>request(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    async function login(){
      const created=await post('/v1/pair/create',{root_folder_id:'root_fixture',device_secret:'a'.repeat(43)});
      assert.equal(created.status,200);const pairing=await created.json();
      const page=await request('/pair/'+pairing.pairing_id);
      const cookie=page.headers.get('Set-Cookie').split(';')[0];
      const nonce=(await page.text()).match(/name="nonce" value="([^"]+)"/)[1];
      const authorized=await request('/pair/'+pairing.pairing_id+'/authorize',{method:'POST',redirect:'manual',headers:{Origin:origin,Cookie:cookie},body:new URLSearchParams({nonce})});
      assert.equal(authorized.status,303);
      const state=new URL(authorized.headers.get('Location')).searchParams.get('state');
      const callback=await request('/oauth/callback?'+new URLSearchParams({state,code:'MOCK_CODE'}),{headers:{Cookie:cookie}});
      return {pairing,callback};
    }
    return {mf,db,calls,request,post,login};
  }catch(error){await mf.dispose();throw error;}
}

test('workerd: Google exchange, folder check, explicit claim and refresh use real fetch',async()=>{
  const f=await fixture();try{
    const {pairing,callback}=await f.login();
    assert.equal(callback.status,200,await callback.text());
    const proof={pairing_id:pairing.pairing_id,device_secret:'a'.repeat(43)};
    const claim=await f.post('/v1/pair/claim',proof);assert.equal(claim.status,200);
    const account=await claim.json();assert.equal(account.refresh_token,'MOCK_REFRESH');
    const refreshed=await f.request('/v1/token',{method:'POST',body:new URLSearchParams({client_id:client,grant_type:'refresh_token',refresh_token:account.refresh_token})});
    assert.equal(refreshed.status,200);assert.equal((await refreshed.json()).access_token,'MOCK_RENEWED');
    assert.equal(f.calls.length,3);
    assert.equal((await f.post('/v1/pair/ack',proof)).status,200);
    assert.equal((await f.db.prepare('SELECT count(*) AS n FROM pairings').first()).n,0);
  }finally{await f.mf.dispose();}
});

test('workerd: Google redirects never forward OAuth secrets or bearer tokens',async()=>{
  for(const mode of ['token_redirect','folder_redirect']){
    const f=await fixture(mode);try{
      const {callback}=await f.login();assert.equal(callback.status,403);
      assert.ok(!(await callback.text()).includes('MOCK_'));
      assert.equal(f.calls.some(url=>url.startsWith('https://forbidden.example')),false);
      assert.equal(f.calls.length,mode==='token_redirect'?1:2);
      const row=await f.db.prepare('SELECT status,token_cipher,oauth_cipher FROM pairings').first();
      assert.equal(row.status,'error');assert.equal(row.token_cipher,null);assert.equal(row.oauth_cipher,null);
    }finally{await f.mf.dispose();}
  }
});
