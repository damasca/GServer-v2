#pragma once

#ifndef CSCRIPTENGINE_H
#define CSCRIPTENGINE_H

#include <cassert>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ScriptAction.h"
#include "ScriptEnv.h"
#include "ScriptFactory.h"
#include "ScriptWrapped.h"

class IScriptEnv;
class IScriptFunction;

class TNPC;
class TServer;
class TWeapon;

class CScriptEngine
{
public:
	CScriptEngine(TServer *server);
	~CScriptEngine();

	bool Initialize();
	void Cleanup();
	void RunScripts(bool timedCall = false);

	TServer * getServer() const;
	IScriptEnv * getScriptEnv() const;
	IScriptWrapped<TServer> * getServerObject() const;

	bool ExecuteNpc(TNPC *npc);
	bool ExecuteWeapon(TWeapon *weapon);

	void RegisterNpcTimer(TNPC *npc);
	void RegisterNpcUpdate(TNPC *npc);
	void RegisterWeaponUpdate(TWeapon *weapon);

	void UnregisterNpcTimer(TNPC *npc);
	void UnregisterNpcUpdate(TNPC *npc);
	void UnregisterWeaponUpdate(TWeapon *weapon);

	// callbacks
	IScriptFunction * getCallBack(const std::string& callback) const;
	void setCallBack(const std::string& callback, IScriptFunction *cbFunc);

	// Script Compile / Cache
	IScriptFunction * CompileCache(const std::string& code, bool referenceCount = true);
	bool ClearCache(const std::string& code);

	const ScriptRunError& getScriptError() const;

	template<class... Args>
	ScriptAction * CreateAction(const std::string& action, Args... An);

	template<class T>
	IScriptWrapped<T> * WrapObject(T *obj) const;

	template <typename T>
	static std::string WrapScript(const std::string& code);

protected:
	std::unordered_map<std::string, IScriptFunction *> _cachedScripts;
	std::unordered_map<std::string, IScriptFunction *> _callbacks;
	std::unordered_set<TNPC *> _updateNpcs;
	std::unordered_set<TNPC *> _updateNpcsTimer;
	std::unordered_set<TWeapon *> _updateWeapons;
	std::vector<ScriptAction *> _actions;

private:
	IScriptEnv *_env;
	IScriptFunction *_bootstrapFunction;
	IScriptWrapped<TServer> *_serverObject;
	IScriptWrapped<TServer> *_environmentObject;
	TServer *_server;
};

// Getters

inline TServer * CScriptEngine::getServer() const {
	return _server;
}

inline IScriptEnv * CScriptEngine::getScriptEnv() const {
	return _env;
}

inline IScriptWrapped<TServer> * CScriptEngine::getServerObject() const {
	return _serverObject;
}

inline IScriptFunction * CScriptEngine::getCallBack(const std::string& callback) const {
	auto it = _callbacks.find(callback);
	if (it != _callbacks.end())
		return it->second;

	return 0;
}

inline const ScriptRunError& CScriptEngine::getScriptError() const {
	return _env->getScriptError();
}

// Setters

inline void CScriptEngine::setCallBack(const std::string& callback, IScriptFunction *cbFunc) {
	_callbacks[callback] = cbFunc;
}

// Register scripts for processing

inline void CScriptEngine::RegisterNpcTimer(TNPC *npc) {
	_updateNpcsTimer.insert(npc);
}

inline void CScriptEngine::RegisterNpcUpdate(TNPC *npc) {
	_updateNpcs.insert(npc);
}

inline void CScriptEngine::RegisterWeaponUpdate(TWeapon *weapon) {
	_updateWeapons.insert(weapon);
}

// Unregister scripts from processing

inline void CScriptEngine::UnregisterWeaponUpdate(TWeapon *weapon) {
	_updateWeapons.erase(weapon);
}

inline void CScriptEngine::UnregisterNpcUpdate(TNPC *npc) {
	_updateNpcs.erase(npc);
}

inline void CScriptEngine::UnregisterNpcTimer(TNPC *npc) {
	_updateNpcsTimer.erase(npc);
}

//

template<class... Args>
ScriptAction * CScriptEngine::CreateAction(const std::string& action, Args... An)
{
	// TODO(joey): This just creates an action, and leaves it up to the user to do something with this. Most-likely will be renamed, or changed.
	constexpr size_t Argc = (sizeof...(Args));
	assert(Argc > 0);

	V8ENV_D("Server_RegisterAction:\n");
	V8ENV_D("\tAction: %s\n", action.c_str());
	V8ENV_D("\tArguments: %zu\n", Argc);

	auto funcIt = _callbacks.find(action);
	if (funcIt == _callbacks.end())
	{
		V8ENV_D("Global::Server_RegisterAction: Callback not registered for %s\n", action.c_str());
		return 0;
	}

	// total temp
	IScriptArguments *args = ScriptArgumentsFactory::Create((V8ScriptEnv *)nullptr, std::forward<Args>(An)...);

	ScriptAction *newScriptAction = new ScriptAction(funcIt->second, args, action);
	return newScriptAction;
}

template<class T>
inline IScriptWrapped<T> * CScriptEngine::WrapObject(T *obj) const
{
	V8ENV_D("Begin Global::WrapObject()\n");

	V8ScriptEnv *env = static_cast<V8ScriptEnv *>(_env);

	// Wrap object, and set the object to the class
	IScriptWrapped<T> *wrappedObject = env->Wrap(ScriptConstructorId<T>::result, obj);
	obj->setScriptObject(wrappedObject);

	V8ENV_D("End Global::WrapObject()\n\n");
	return wrappedObject;
}

template <typename T>
inline std::string CScriptEngine::WrapScript(const std::string& code) {
	return code;
}

template <>
inline std::string CScriptEngine::WrapScript<TNPC>(const std::string& code) {
	// self.onCreated || onCreated, for first declared to take precedence
	// if (onCreated) for latest function to override
	static const char *prefixString = "(function(npc) {" \
		"var onCreated, onTimeout, onPlayerChats, onPlayerEnters, onPlayerLeaves, onPlayerTouchsMe, onPlayerLogin, onPlayerLogout;" \
		"const self = npc;" \
		"if (onCreated) self.onCreated = onCreated;" \
		"if (onTimeout) self.onTimeout = onTimeout;" \
		"if (onPlayerChats) self.onPlayerChats = onPlayerChats;" \
		"if (onPlayerEnters) self.onPlayerEnters = onPlayerEnters;" \
		"if (onPlayerLeaves) self.onPlayerLeaves = onPlayerLeaves;" \
		"if (onPlayerTouchsMe) self.onPlayerTouchsMe = onPlayerTouchsMe;" \
		"if (onPlayerLogin) self.onPlayerLogin = onPlayerLogin;" \
		"if (onPlayerLogout) self.onPlayerLogout = onPlayerLogout;\n";

	std::string wrappedCode = std::string(prefixString);
	wrappedCode.append(code);
	wrappedCode.append("});");
	return wrappedCode;
}

template <>
inline std::string CScriptEngine::WrapScript<TPlayer>(const std::string& code) {
	static const char *prefixString = "(function(player) {" \
		"const self = player;\n";

	std::string wrappedCode = std::string(prefixString);
	wrappedCode.append(code);
	wrappedCode.append("});");
	return wrappedCode;
}

template <>
inline std::string CScriptEngine::WrapScript<TWeapon>(const std::string& code) {
	static const char *prefixString = "(function(weapon) {" \
		"var onCreated, onActionServerSide;" \
		"const self = weapon;" \
		"self.onCreated = onCreated;" \
		"self.onActionServerSide = onActionServerSide;\n";

	std::string wrappedCode = std::string(prefixString);
	wrappedCode.append(code);
	wrappedCode.append("});");
	return wrappedCode;
}

#endif
