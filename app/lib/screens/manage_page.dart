import 'package:flutter/material.dart';

import '../state/bond_state.dart';

class ManagePage extends StatefulWidget {
  const ManagePage({super.key, required this.state});

  final BondState state;

  @override
  State<ManagePage> createState() => _ManagePageState();
}

class _ManagePageState extends State<ManagePage> {
  late final TextEditingController _persona;
  late final TextEditingController _notes;
  late final TextEditingController _baseUrl;

  @override
  void initState() {
    super.initState();
    _persona = TextEditingController(text: widget.state.persona);
    _notes = TextEditingController(text: widget.state.notes);
    _baseUrl = TextEditingController(text: widget.state.api.baseUrl);
    widget.state.addListener(_sync);
  }

  void _sync() {
    if (_persona.text != widget.state.persona && !_persona.selection.isValid) {
      _persona.text = widget.state.persona;
    }
    if (_notes.text != widget.state.notes && !_notes.selection.isValid) {
      _notes.text = widget.state.notes;
    }
  }

  @override
  void dispose() {
    widget.state.removeListener(_sync);
    _persona.dispose();
    _notes.dispose();
    _baseUrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = widget.state;
    return ListenableBuilder(
      listenable: state,
      builder: (context, _) {
        return ListView(
          padding: const EdgeInsets.fromLTRB(20, 16, 20, 32),
          children: [
            Text(
              state.online ? '云端已连接' : '云端离线 · 闹钟仍走本地',
              style: TextStyle(
                color: state.online ? const Color(0xFF2ECC71) : Colors.orangeAccent,
              ),
            ),
            if (state.lastError.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Text(state.lastError, style: const TextStyle(color: Colors.redAccent)),
              ),
            const SizedBox(height: 16),
            _card(
              title: '云端地址',
              child: Column(
                children: [
                  TextField(
                    controller: _baseUrl,
                    decoration: const InputDecoration(
                      labelText: 'API',
                      hintText: '模拟器 10.0.2.2  真机填电脑局域网 IP',
                    ),
                  ),
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerRight,
                    child: FilledButton(
                      onPressed: () {
                        state.api.baseUrl = _baseUrl.text.trim();
                        state.boot();
                      },
                      child: const Text('连接'),
                    ),
                  ),
                ],
              ),
            ),
            _card(
              title: '绑定',
              child: Text('配对码 ${state.pairCode.isEmpty ? "----" : state.pairCode} · ${state.displayName}\n${state.bound ? "已绑定到当前账号" : "未绑定"}'),
            ),
            _card(
              title: '安装包 / 更新',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('当前 ${state.appVersion.isEmpty ? "读取中" : state.appVersion}'),
                  if (state.downloadPage.isNotEmpty)
                    Padding(
                      padding: const EdgeInsets.only(top: 8),
                      child: SelectableText(state.downloadPage),
                    ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    children: [
                      FilledButton.tonal(
                        onPressed: state.downloadPage.isEmpty && state.apkUrl == null
                            ? null
                            : state.openUpdate,
                        child: const Text('打开下载页'),
                      ),
                    ],
                  ),
                ],
              ),
            ),
            _card(
              title: '人设 / 记忆',
              child: Column(
                children: [
                  TextField(
                    controller: _persona,
                    maxLines: 2,
                    decoration: const InputDecoration(labelText: '人设'),
                    onChanged: (value) => state.persona = value,
                  ),
                  TextField(
                    controller: _notes,
                    maxLines: 3,
                    decoration: const InputDecoration(labelText: '长期记忆'),
                    onChanged: (value) => state.notes = value,
                  ),
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerRight,
                    child: FilledButton(
                      onPressed: state.online ? state.saveMemory : null,
                      child: const Text('保存到云端'),
                    ),
                  ),
                ],
              ),
            ),
            _card(
              title: '音量 / 勿扰 / 主动开口',
              child: Column(
                children: [
                  Slider(
                    value: state.volume.toDouble(),
                    max: 100,
                    label: '${state.volume}',
                    onChanged: (value) => state.setVolume(value.round()),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('勿扰'),
                    value: state.dnd,
                    onChanged: (_) => state.toggleDnd(),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('允许主动说话'),
                    subtitle: const Text('关掉后除本地闹钟外不再主动出声'),
                    value: state.allowProactive,
                    onChanged: state.setProactive,
                  ),
                ],
              ),
            ),
            _card(
              title: '闹钟（到点本地响）',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  FilledButton.tonal(
                    onPressed: state.addOneMinuteAlarm,
                    child: const Text('1 分钟后叫我'),
                  ),
                  const SizedBox(height: 8),
                  if (state.alarms.isEmpty) const Text('还没有闹钟'),
                  for (final alarm in state.alarms)
                    ListTile(
                      contentPadding: EdgeInsets.zero,
                      dense: true,
                      title: Text(alarm.label),
                      subtitle: Text(alarm.fireAt.toLocal().toString().substring(0, 19)),
                    ),
                ],
              ),
            ),
            _card(
              title: '同一线程历史',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (state.history.isEmpty) const Text('还没有对话'),
                  for (final item in state.history.take(12))
                    Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: Text(
                        '${item['role'] == 'user' ? '你' : '表'} · ${item['text']}',
                      ),
                    ),
                ],
              ),
            ),
          ],
        );
      },
    );
  }

  Widget _card({required String title, required Widget child}) {
    return Card(
      margin: const EdgeInsets.only(bottom: 16),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: const TextStyle(fontWeight: FontWeight.w700)),
            const SizedBox(height: 8),
            child,
          ],
        ),
      ),
    );
  }
}
