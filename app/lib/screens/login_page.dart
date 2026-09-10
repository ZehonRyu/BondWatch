import 'package:flutter/material.dart';

import '../state/bond_state.dart';

class LoginPage extends StatefulWidget {
  const LoginPage({super.key, required this.state});

  final BondState state;

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final _user = TextEditingController(text: 'demo');
  final _pass = TextEditingController(text: 'demo123');
  final _name = TextEditingController();
  final _api = TextEditingController();
  bool _register = false;

  @override
  void initState() {
    super.initState();
    _api.text = widget.state.api.baseUrl;
  }

  @override
  void dispose() {
    _user.dispose();
    _pass.dispose();
    _name.dispose();
    _api.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.state,
      builder: (context, _) {
        return Scaffold(
          body: Container(
            decoration: const BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: [Color(0xFF10182C), Color(0xFF07080C), Color(0xFF2A1D0C)],
              ),
            ),
            child: Center(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 420),
                child: Card(
                  color: const Color(0xCC141824),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(28)),
                  child: Padding(
                    padding: const EdgeInsets.all(28),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('BONDWATCH', style: TextStyle(letterSpacing: 3, color: Colors.white54)),
                        const SizedBox(height: 8),
                        Text(
                          _register ? '建一个账号' : '回到这只表',
                          style: const TextStyle(fontSize: 32, fontWeight: FontWeight.w500),
                        ),
                        const SizedBox(height: 20),
                        TextField(controller: _user, decoration: const InputDecoration(labelText: '用户名')),
                        TextField(
                          controller: _pass,
                          obscureText: true,
                          decoration: const InputDecoration(labelText: '密码'),
                        ),
                        if (_register)
                          TextField(controller: _name, decoration: const InputDecoration(labelText: '怎么称呼')),
                        TextField(controller: _api, decoration: const InputDecoration(labelText: '云端地址')),
                        if (widget.state.lastError.isNotEmpty)
                          Padding(
                            padding: const EdgeInsets.only(top: 12),
                            child: Text(widget.state.lastError, style: const TextStyle(color: Colors.redAccent)),
                          ),
                        const SizedBox(height: 20),
                        SizedBox(
                          width: double.infinity,
                          child: FilledButton(
                            onPressed: () {
                              widget.state.api.baseUrl = _api.text.trim();
                              widget.state.signIn(
                                username: _user.text.trim(),
                                password: _pass.text,
                                name: _name.text.trim(),
                                register: _register,
                              );
                            },
                            child: Text(_register ? '注册并进入' : '登录'),
                          ),
                        ),
                        TextButton(
                          onPressed: () => setState(() => _register = !_register),
                          child: Text(_register ? '已有账号，去登录' : '没有账号，去注册'),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ),
        );
      },
    );
  }
}
