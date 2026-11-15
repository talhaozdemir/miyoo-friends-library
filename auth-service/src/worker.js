// No runtime dependencies, analytics, request-body logs, ROM proxy or uploads.
const SCOPE = 'https://www.googleapis.com/auth/drive.readonly';
const TOKEN_URL = 'https://oauth2.googleapis.com/token';
const enc = new TextEncoder();
/** @typedef {{id:string,device_mac:string,display_code:string,expires:number,status:string,browser_mac:string|null,state_hash:string|null,oauth_cipher:string|null,token_cipher:string|null,error_code:string|null,last_poll:number}} Pair */
/** @typedef {{access_token:string,refresh_token:string,token_type:string,expires_in:number,issued_at:number}} Tokens */
/** @typedef {{aes:CryptoKey,mac:CryptoKey}} Keys */

/** @param {Uint8Array} bytes */
export function b64(bytes) { return btoa(String.fromCharCode(...bytes)).replaceAll('+','-').replaceAll('/','_').replaceAll('=',''); }
/** @param {string} text */
function un64(text) { if(!/^[A-Za-z0-9_-]+$/.test(text)||text.length>65536)throw new Error('encoding'); return Uint8Array.from(atob(text.replaceAll('-','+').replaceAll('_','/')),c=>c.charCodeAt(0)); }
/** @param {number} [length] */
function random(length=32) { return b64(crypto.getRandomValues(new Uint8Array(length))); }
/** @param {string} value */
export async function digest(value) { return b64(new Uint8Array(await crypto.subtle.digest('SHA-256',enc.encode(value)))); }
/** @param {Env} env @returns {Promise<Keys>} */
async function keys(env) {
  const bytes=un64(env.SESSION_KEY); if(bytes.length!==32)throw new Error('key');
  const root=await crypto.subtle.importKey('raw',bytes,'HKDF',false,['deriveKey']);
  const salt=enc.encode(env.PUBLIC_ORIGIN);
  const aes=await crypto.subtle.deriveKey({name:'HKDF',hash:'SHA-256',salt,info:enc.encode('mfl-encryption-v1')},root,{name:'AES-GCM',length:256},false,['encrypt','decrypt']);
  const mac=await crypto.subtle.deriveKey({name:'HKDF',hash:'SHA-256',salt,info:enc.encode('mfl-proof-v1')},root,{name:'HMAC',hash:'SHA-256',length:256},false,['sign','verify']);
  return {aes,mac};
}
/** @param {Keys} key @param {string} value */
async function sign(key,value) { return b64(new Uint8Array(await crypto.subtle.sign('HMAC',key.mac,enc.encode(value)))); }
/** @param {Keys} key @param {string} value @param {string|null} mac */
async function proof(key,value,mac) { try{return !!mac&&await crypto.subtle.verify('HMAC',key.mac,un64(mac),enc.encode(value));}catch{return false;} }
/** @param {Keys} key @param {string} context @param {unknown} value */
export async function seal(key,context,value) {
  const iv=crypto.getRandomValues(new Uint8Array(12));
  const cipher=await crypto.subtle.encrypt({name:'AES-GCM',iv,additionalData:enc.encode(context)},key.aes,enc.encode(JSON.stringify(value)));
  return b64(iv)+'.'+b64(new Uint8Array(cipher));
}
/** @param {Keys} key @param {string} context @param {string} value @returns {Promise<unknown>} */
export async function open(key,context,value) {
  const [iv,cipher,...extra]=value.split('.');if(extra.length||!iv||!cipher)throw new Error('cipher');
  const data=await crypto.subtle.decrypt({name:'AES-GCM',iv:un64(iv),additionalData:enc.encode(context)},key.aes,un64(cipher));
  return JSON.parse(new TextDecoder().decode(data));
}
/** @param {Request|Response} message @param {number} [limit] */
async function bounded(message,limit=16384) {
  const reader=message.body?.getReader(); if(!reader)return '';
  const chunks=[];let count=0;
  try{for(;;){const {value,done}=await reader.read();if(done)break;count+=value.length;if(count>limit)throw new Error('body_limit');chunks.push(value);}}
  finally{await reader.cancel().catch(()=>{});reader.releaseLock();}
  const bytes=new Uint8Array(count);let pos=0;for(const c of chunks){bytes.set(c,pos);pos+=c.length;}return new TextDecoder().decode(bytes);
}
/** @param {unknown} v @returns {v is Record<string,unknown>} */
function object(v) { return typeof v==='object'&&v!==null&&!Array.isArray(v); }
/** @param {unknown} v @returns {v is string} */
function code(v) { return typeof v==='string'&&/^[A-Za-z0-9_-]{43}$/.test(v); }
/** @param {unknown} v @returns {v is Tokens} */
function tokens(v) { return object(v)&&typeof v.access_token==='string'&&/^[A-Za-z0-9._~+/=-]{1,8192}$/.test(v.access_token)&&typeof v.refresh_token==='string'&&v.refresh_token.length>0&&v.refresh_token.length<=4096&&v.token_type==='Bearer'&&typeof v.expires_in==='number'&&v.expires_in>0&&v.expires_in<=86400&&typeof v.issued_at==='number'; }
/** @param {unknown} data @param {number} [status] @param {Record<string,string>} [extra] */
function json(data,status=200,extra={}) { return new Response(JSON.stringify(data),{status,headers:{...security(),'Content-Type':'application/json; charset=utf-8',...extra}}); }
function security() { return {'Cache-Control':'no-store, max-age=0','Pragma':'no-cache','Referrer-Policy':'same-origin','X-Content-Type-Options':'nosniff','X-Frame-Options':'DENY','Content-Security-Policy':"default-src 'none'; style-src 'unsafe-inline'; form-action 'self' https://accounts.google.com; frame-ancestors 'none'; base-uri 'none'",'Strict-Transport-Security':'max-age=31536000'}; }
/** @param {string} text */
function escape(text) { return text.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;').replaceAll("'",'&#39;'); }
/** @param {string} title @param {string} content @param {Record<string,string>} [extra] @param {number} [status] */
function html(title,content,extra={},status=200) {
  return new Response(`<!doctype html><html lang="tr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="robots" content="noindex,nofollow"><title>${escape(title)} · Friends Library</title><style>body{background:#101923;color:#ecf4f7;font:18px/1.6 system-ui;margin:0}main{max-width:540px;margin:8vh auto;padding:24px}h1{font-size:30px;line-height:1.25}button{background:#64d9ba;border:0;border-radius:12px;font:inherit;font-weight:650;padding:16px 24px;color:#10251f;width:100%}.code{font:36px monospace;letter-spacing:7px;color:#64d9ba}small{color:#b7c8d3}a{color:#64d9ba}</style><main><small>MIYOO FRIENDS LIBRARY</small><h1>${escape(title)}</h1>${content}</main></html>`,{status,headers:{...security(),'Content-Type':'text/html; charset=utf-8',...extra}});
}
/** @param {string} to @param {Record<string,string>} [extra] */
function redirect(to,extra={}) { return new Response(null,{status:303,headers:{...security(),Location:to,...extra}}); }
/** @param {D1Database} db @param {string} id @returns {Promise<Pair|null>} */
function pair(db,id) { return db.prepare('SELECT * FROM pairings WHERE id = ?').bind(id).first(); }
/** @param {D1Database} db @param {string} bucket @param {number} max @param {number} expires */
async function limited(db,bucket,max,expires) {
  const row=await db.prepare('INSERT INTO rate_limits(bucket,count,expires) VALUES (?,1,?) ON CONFLICT(bucket) DO UPDATE SET count=count+1 RETURNING count').bind(bucket,expires).first();
  return row&&typeof row.count==='number'&&row.count<=max;
}
/** @param {Env} env @param {number} now */
async function prune(env,now) {
  await env.DB.batch([env.DB.prepare('DELETE FROM pairings WHERE id IN (SELECT id FROM pairings WHERE expires < ? LIMIT 100)').bind(now),env.DB.prepare('DELETE FROM rate_limits WHERE bucket IN (SELECT bucket FROM rate_limits WHERE expires < ? LIMIT 100)').bind(now)]);
}
/** @param {Request} request @param {Keys} key @param {number} now @returns {Promise<{id:string,nonce:string,expires:number}|null>} */
async function cookie(request,key,now) {
  const match=(request.headers.get('Cookie')||'').match(/(?:^|;\s*)__Host-mfl=([A-Za-z0-9_.-]+)/);if(!match)return null;
  const [payload,mac,...extra]=match[1].split('.');if(extra.length||!await proof(key,payload,mac))return null;
  try{const v=JSON.parse(new TextDecoder().decode(un64(payload)));return object(v)&&code(v.id)&&code(v.nonce)&&typeof v.expires==='number'&&v.expires>now?{id:v.id,nonce:v.nonce,expires:v.expires}:null;}catch{return null;}
}
/** @param {Keys} key @param {string} id @param {number} expires */
async function setCookie(key,id,expires) {
  const nonce=random(),payload=b64(enc.encode(JSON.stringify({id,nonce,expires})));
  return {nonce,header:`__Host-mfl=${payload}.${await sign(key,payload)}; Secure; HttpOnly; SameSite=Lax; Path=/; Max-Age=300`};
}
/** @param {Request} request @param {Env} env @param {Keys} key @param {number} now */
async function device(request,env,key,now) {
  const v=JSON.parse(await bounded(request));if(!object(v)||!code(v.pairing_id)||!code(v.device_secret))return null;
  const p=await pair(env.DB,v.pairing_id);if(!p||p.expires<=now||!await proof(key,`${p.id}:${v.device_secret}`,p.device_mac))return null;
  return p;
}
/** @param {{fetchImpl?:typeof fetch,clock?:()=>number}} [options] */
export function createHandler(options={}) {
  const call=options.fetchImpl||fetch,clock=options.clock||(()=>Date.now());
  /** @param {Env} env @param {URL} url @param {URLSearchParams} body */
  async function google(env,url,body) {
    const response=await call(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body,redirect:'error',signal:AbortSignal.timeout(12000)});
    const data=JSON.parse(await bounded(response));return {ok:response.ok,status:response.status,data};
  }
  /** @param {Env} env @param {string} access */
  async function folder(env,access) {
    const u=new URL(`https://www.googleapis.com/drive/v3/files/${env.ROOT_FOLDER_ID}`);u.searchParams.set('fields','id,mimeType');u.searchParams.set('supportsAllDrives','true');
    const response=await call(u,{headers:{Authorization:`Bearer ${access}`},redirect:'error',signal:AbortSignal.timeout(12000)});
    const v=JSON.parse(await bounded(response));return response.ok&&object(v)&&v.id===env.ROOT_FOLDER_ID&&v.mimeType==='application/vnd.google-apps.folder';
  }
  return {
    /** @param {Request} request @param {Env} env */
    async fetch(request,env) {
      const now=Math.floor(clock()/1000),url=new URL(request.url),path=url.pathname;
      try{
        if(path==='/health'&&request.method==='GET')return json({service:'miyoo-friends-auth',protocol:1});
        if(!/^https:\/\/[a-z0-9.-]+$/.test(env.PUBLIC_ORIGIN)||env.PUBLIC_ORIGIN.includes('REPLACE')||!env.GOOGLE_CLIENT_SECRET||!env.SESSION_KEY||!/^[-\w]{1,159}$/.test(env.ROOT_FOLDER_ID)||env.ROOT_FOLDER_ID==='REPLACE'||!env.GOOGLE_CLIENT_ID.endsWith('.apps.googleusercontent.com'))return json({error:'service_not_configured'},503);
        if(url.origin!==env.PUBLIC_ORIGIN||url.protocol!=='https:')return json({error:'wrong_origin'},400);
        const key=await keys(env);
        if(path==='/v1/pair/create'&&request.method==='POST'){
          if(request.headers.has('Origin'))return json({error:'device_only'},403);
          const body=JSON.parse(await bounded(request));if(!object(body)||!code(body.device_secret)||body.root_folder_id!==env.ROOT_FOLDER_ID)return json({error:'invalid_request'},400);
          const day=Math.floor(now/86400),ip=await sign(key,request.headers.get('CF-Connecting-IP')||'local');
          if(!await limited(env.DB,`global:${day}`,100,(day+1)*86400)||!await limited(env.DB,`ip:${day}:${ip}`,15,(day+1)*86400))return json({error:'rate_limited'},429,{'Retry-After':'60'});
          await prune(env,now);
          const id=random(),display=String(crypto.getRandomValues(new Uint32Array(1))[0]%1000000).padStart(6,'0');
          await env.DB.prepare('INSERT INTO pairings(id,device_mac,display_code,expires,status) VALUES (?,?,?,?,?)').bind(id,await sign(key,`${id}:${body.device_secret}`),display,now+300,'pending').run();
          return json({pairing_id:id,verification_uri:`${env.PUBLIC_ORIGIN}/pair/${id}`,display_code:display,expires_in:300,interval:5});
        }
        if(['/v1/pair/status','/v1/pair/claim','/v1/pair/cancel','/v1/pair/ack'].includes(path)&&request.method==='POST'){
          if(request.headers.has('Origin'))return json({error:'device_only'},403);
          const p=await device(request,env,key,now);if(!p)return json({error:'pairing_expired_or_invalid'},404);
          if(path.endsWith('/cancel')||path.endsWith('/ack')){
            await env.DB.prepare('DELETE FROM pairings WHERE id=?').bind(p.id).run();return json({status:'closed'});
          }
          if(path.endsWith('/status')){
            if(p.last_poll&&now-p.last_poll<4)return json({error:'slow_down'},429,{'Retry-After':'5'});
            await env.DB.prepare('UPDATE pairings SET last_poll=? WHERE id=?').bind(now,p.id).run();
            return json({status:p.status==='claimed'?'ready':p.status,display_code:p.display_code,error:p.error_code||undefined});
          }
          if(!['ready','claimed'].includes(p.status)||!p.token_cipher)return json({error:'authorization_pending'},409);
          const v=await open(key,`tokens:${p.id}`,p.token_cipher);if(!tokens(v))throw new Error('tokens');
          const remaining=v.expires_in-(now-v.issued_at);if(remaining<60)return json({error:'pairing_expired_or_invalid'},410);
          const rtHash=await digest(v.refresh_token);
          const claimed=await env.DB.batch([env.DB.prepare("INSERT OR IGNORE INTO grants(digest,pairing_id,created) SELECT ?,id,? FROM pairings WHERE id=? AND status IN ('ready','claimed') AND expires>?").bind(rtHash,now,p.id,now),env.DB.prepare("UPDATE pairings SET status='claimed' WHERE id=? AND status IN ('ready','claimed') AND expires>?").bind(p.id,now)]);
          if(!claimed[1].meta.changes)return json({error:'pairing_expired_or_invalid'},404);
          return json({access_token:v.access_token,refresh_token:v.refresh_token,token_type:'Bearer',expires_in:remaining,client_id:env.GOOGLE_CLIENT_ID,root_folder_id:env.ROOT_FOLDER_ID,token_url:`${env.PUBLIC_ORIGIN}/v1/token`});
        }
        const match=path.match(/^\/pair\/([A-Za-z0-9_-]{43})(\/authorize)?$/);
        if(match){
          const p=await pair(env.DB,match[1]);if(!p||p.expires<=now)return html('Kodun süresi doldu','<p>Miyoo’da yeni bir giriş QR’si oluştur.</p>',{},410);
          if(!match[2]&&request.method==='GET'){
            if(p.status!=='pending')return html('Giriş devam ediyor','<p>Miyoo ekranına dön. Başka bir giriş başlatmak için yeni QR oluştur.</p>');
            const c=await setCookie(key,p.id,p.expires);
            return html('Miyoo’nu Google’a bağla',`<p>Bu kod Miyoo ekranındakiyle aynı mı?</p><p class="code">${p.display_code}</p><p>Yalnız kendi cihazında başlattığın bağlantıyı onayla. Drive klasörü özel kalır; salt okunur erişim istenir.</p><form method="post" action="/pair/${p.id}/authorize"><input type="hidden" name="nonce" value="${c.nonce}"><button>Google ile devam et</button></form><p><small>Google bağlantısı bu HTTPS servisi üzerinden yenilenecek. Oyunlar doğrudan Drive’dan Miyoo’ya iner.</small></p>`,{'Set-Cookie':c.header});
          }
          if(match[2]&&request.method==='POST'){
            const c=await cookie(request,key,now),body=new URLSearchParams(await bounded(request));
            const nonce=body.get('nonce');
            if(request.headers.get('Origin')!==env.PUBLIC_ORIGIN||!c||c.id!==p.id||!nonce||!await proof(key,nonce,await sign(key,c.nonce)))return html('Tarayıcı oturumu yenilenmeli',`<p>Giriş sayfasını telefonunda Safari veya Chrome ile yeniden aç. Kamera önizlemesi yerine normal tarayıcıyı kullan.</p><p><a href="/pair/${p.id}">Giriş sayfasını yeniden aç</a></p><p>Sorun sürerse Miyoo’da B ile geri dönüp yeni bir QR oluştur. Önceki hesabın değiştirilmedi.</p>`,{},403);
            const state=random(),verifier=random();
            const changed=await env.DB.prepare("UPDATE pairings SET status='authorizing',browser_mac=?,state_hash=?,oauth_cipher=? WHERE id=? AND status='pending'").bind(await sign(key,c.nonce),await sign(key,state),await seal(key,`oauth:${p.id}`,{verifier}),p.id).run();
            if(!changed.meta.changes)return json({error:'login_already_started'},409);
            const u=new URL('https://accounts.google.com/o/oauth2/v2/auth');
            u.search=new URLSearchParams({client_id:env.GOOGLE_CLIENT_ID,redirect_uri:`${env.PUBLIC_ORIGIN}/oauth/callback`,response_type:'code',scope:SCOPE,access_type:'offline',prompt:'consent',include_granted_scopes:'false',state,code_challenge:await digest(verifier),code_challenge_method:'S256'}).toString();return redirect(u.toString());
          }
        }
        if(path==='/oauth/callback'&&request.method==='GET'){
          const c=await cookie(request,key,now),state=url.searchParams.get('state');if(!c||!code(state))return html('Giriş doğrulanamadı','<p>Miyoo’da yeni bir QR oluştur.</p>',{},400);
          const p=await pair(env.DB,c.id);
          if(!p||p.expires<=now||p.status!=='authorizing'||!await proof(key,state,p.state_hash)||!await proof(key,c.nonce,p.browser_mac)||!p.oauth_cipher)return html('Geçersiz veya kullanılmış kod','<p>Miyoo’da yeni bir giriş başlat.</p>',{},400);
          const changed=await env.DB.prepare("UPDATE pairings SET status='processing',state_hash=NULL WHERE id=? AND status='authorizing' AND state_hash=?").bind(p.id,p.state_hash).run();
          if(!changed.meta.changes)return json({error:'callback_replayed'},409);
          try{
            if(url.searchParams.has('error'))throw new Error('authorization_declined');
            const authorizationCode=url.searchParams.get('code');if(!authorizationCode||authorizationCode.length>4096)throw new Error('authorization_failed');
            const pkce=await open(key,`oauth:${p.id}`,p.oauth_cipher);if(!object(pkce)||typeof pkce.verifier!=='string')throw new Error('authorization_failed');
            let g;
            try{g=await google(env,new URL(TOKEN_URL),new URLSearchParams({grant_type:'authorization_code',code:authorizationCode,client_id:env.GOOGLE_CLIENT_ID,client_secret:env.GOOGLE_CLIENT_SECRET.trim(),redirect_uri:`${env.PUBLIC_ORIGIN}/oauth/callback`,code_verifier:pkce.verifier}));}
            catch{throw new Error('google_temporarily_unavailable');}
            if(!g.ok)throw new Error(object(g.data)&&g.data.error==='invalid_client'?'oauth_client_configuration':object(g.data)&&g.data.error==='invalid_grant'?'authorization_code_rejected':'authorization_failed');
            if(!object(g.data)||typeof g.data.scope!=='string'||g.data.scope.trim()!==SCOPE)throw new Error('readonly_permission_missing');
            const v={...g.data,issued_at:now};if(!tokens(v))throw new Error('refresh_permission_missing');
            if(!await folder(env,v.access_token))throw new Error('shared_folder_denied');
            const result=await env.DB.prepare("UPDATE pairings SET status='ready',token_cipher=?,oauth_cipher=NULL WHERE id=? AND status='processing' AND expires>?").bind(await seal(key,`tokens:${p.id}`,v),p.id,now).run();
            if(!result.meta.changes)return html('Bağlantı iptal edildi','<p>Miyoo’da yeni bir giriş başlat.</p>',{},410);
            return html('Google onayı tamamlandı',`<p class="code">${p.display_code}</p><p>Miyoo’daki kodla karşılaştır ve cihazında <strong>A</strong> ile bağlantıyı kaydet. Bu sayfada başka işlem yok.</p>`,{'Set-Cookie':'__Host-mfl=; Secure; HttpOnly; SameSite=Lax; Path=/; Max-Age=0'});
          }catch(e){
            /** @type {Record<string,string>} */
            const messages={
              authorization_declined:'Google girişi iptal edildi. Yeni QR ile tekrar deneyebilirsin.',
              oauth_client_configuration:'Giriş servisinin Google ayarları kontrol edilmeli. Hesabını silme; uygulamayı sağlayan kişiye bu hatayı bildir.',
              authorization_code_rejected:'Google giriş kodu kullanılamadı. Miyoo’da yeni QR oluşturup tekrar dene.',
              google_temporarily_unavailable:'Google’a şu an ulaşılamıyor. Biraz sonra yeni QR ile tekrar dene.',
              readonly_permission_missing:'Google Drive okuma izni verilmedi. Yeni QR ile giriş yaparken Drive dosyalarını görüntüleme ve indirme kutusunu işaretle.',
              refresh_permission_missing:'Google kalıcı bağlantı izni vermedi. Yeni QR ile giriş iznini tekrar onayla.',
              shared_folder_denied:'Bu Google hesabı ortak klasöre erişemiyor. Davet edilen hesabı seç.',
              authorization_failed:'Google girişi tamamlanamadı. Yeni QR ile tekrar deneyebilirsin.'
            };
            const reason=e instanceof Error&&Object.hasOwn(messages,e.message)?e.message:'authorization_failed';
            await env.DB.prepare("UPDATE pairings SET status='error',error_code=?,oauth_cipher=NULL,token_cipher=NULL WHERE id=? AND status='processing'").bind(reason,p.id).run();
            return html('Bağlantı kurulamadı',`<p>${messages[reason]}</p><p>Önceki Miyoo hesabın değiştirilmedi.</p><small>Hata: ${reason}</small>`,{},403);
          }
        }
        if(path==='/v1/token'&&request.method==='POST'){
          if(request.headers.has('Origin'))return json({error:'invalid_request'},403);
          const body=new URLSearchParams(await bounded(request));let client=body.get('client_id');
          const basic=request.headers.get('Authorization');if(basic?.startsWith('Basic ')){try{const text=atob(basic.slice(6));client=text.split(':')[0];}catch{return json({error:'invalid_client'},400);}}
          const rt=body.get('refresh_token');if(client!==env.GOOGLE_CLIENT_ID||body.get('grant_type')!=='refresh_token'||!rt||rt.length>4096)return json({error:'invalid_grant'},400);
          const hash=await digest(rt),grant=await env.DB.prepare('SELECT revoked,last_refresh FROM grants WHERE digest=?').bind(hash).first();
          if(!grant||grant.revoked)return json({error:'invalid_grant'},400);
          const reserved=await env.DB.prepare('UPDATE grants SET last_refresh=? WHERE digest=? AND revoked=0 AND last_refresh<=?').bind(now,hash,now-5).run();
          if(!reserved.meta.changes)return json({error:'temporarily_unavailable'},429,{'Retry-After':'5'});
          const g=await google(env,new URL(TOKEN_URL),new URLSearchParams({grant_type:'refresh_token',refresh_token:rt,client_id:env.GOOGLE_CLIENT_ID,client_secret:env.GOOGLE_CLIENT_SECRET.trim()}));
          if(!g.ok){
            const kind=object(g.data)&&g.data.error==='invalid_grant'?'invalid_grant':'temporarily_unavailable';
            if(kind==='invalid_grant')await env.DB.prepare('UPDATE grants SET revoked=1 WHERE digest=?').bind(hash).run();
            return json({error:kind},kind==='invalid_grant'?400:503);
          }
          if(!object(g.data)||typeof g.data.access_token!=='string'||!/^[-A-Za-z0-9._~+/=]{1,8192}$/.test(g.data.access_token)||g.data.token_type!=='Bearer'||typeof g.data.expires_in!=='number'||g.data.expires_in<=0||g.data.expires_in>86400)return json({error:'temporarily_unavailable'},503);
          // Google ordinarily retains the same refresh token; support rotation.
          if(typeof g.data.refresh_token==='string'&&g.data.refresh_token&&g.data.refresh_token!==rt){
            await env.DB.prepare('INSERT OR IGNORE INTO grants(digest,pairing_id,created) SELECT ?,pairing_id,? FROM grants WHERE digest=? AND revoked=0').bind(await digest(g.data.refresh_token),now,hash).run();
          }
          return json({access_token:g.data.access_token,token_type:'Bearer',expires_in:g.data.expires_in,...(typeof g.data.refresh_token==='string'?{refresh_token:g.data.refresh_token}:{})});
        }
        return html('Friends Library','<p>Giriş için kendi Miyoo’nda Connect with phone seç ve QR’yi okut.</p>',{},404);
      }catch{return json({error:'service_unavailable'},503);}
    },
    /** @param {ScheduledController} _event @param {Env} env */
    async scheduled(_event,env){await prune(env,Math.floor(clock()/1000));}
  };
}
export default createHandler();
