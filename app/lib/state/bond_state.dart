import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:image_picker/image_picker.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:url_launcher/url_launcher.dart';

import '../api/client.dart';

class LocalAlarm {
  LocalAlarm({required this.id, required this.fireAt, required this.label});
  final String id;
  final DateTime fireAt;
  final String label;
}

class BondState extends ChangeNotifier {
  BondState({BondApi? api}) : api = api ?? BondApi();

  final BondApi api;

  bool online = false;
  bool signedIn = false;
  bool bound = false;
  bool liveWatch = false;
  String faceSource = '';
  int faceSeq = 0;
  int audioSeq = 0;
  String audioUrl = '';
  double audioSeconds = 0;
  String displayName = '';
  String pairCode = '';
  bool screenOn = true;
  bool dnd = false;
  bool speakingMuted = false;
  String emotion = 'idle';
  String subtitle = '点屏幕或短按对讲';
  String sessionId = '';
  String persona = '';
  String notes = '';
  String lastError = '';
  int volume = 70;
  bool allowProactive = false;
  List<LocalAlarm> alarms = [];
  List<Map<String, String>> history = [];
  bool updateAvailable = false;
  String appVersion = '';
  String updateNotes = '';
  String downloadPage = '';
  String? apkUrl;

  Timer? _ticker;
  Timer? _facePoll;

  Future<void> boot() async {
    _ticker?.cancel();
    _ticker = Timer.periodic(const Duration(seconds: 1), (_) => _tickAlarms());
    _facePoll?.cancel();
    _facePoll = Timer.periodic(const Duration(seconds: 1), (_) {
      _pollFace();
      _pollAudio();
    });
    online = await api.health();
    if (!online) {
      emotion = 'offline';
      subtitle = '没网。闹钟仍会本地响。';
    }
    await checkUpdates();
    notifyListeners();
  }

  Future<void> checkUpdates() async {
    try {
      final info = await PackageInfo.fromPlatform();
      appVersion = '${info.version}+${info.buildNumber}';
      final data = await api.updates();
      final remoteBuild = data['build'] as int? ?? 0;
      final localBuild = int.tryParse(info.buildNumber) ?? 0;
      updateAvailable = remoteBuild > localBuild;
      updateNotes = data['notes']?.toString() ?? '';
      apkUrl = data['apk_url']?.toString();
      downloadPage = data['download_page']?.toString() ?? '';
    } catch (_) {
      updateAvailable = false;
      try {
        final data = await api.updates();
        downloadPage = data['download_page']?.toString() ?? '';
        apkUrl = data['apk_url']?.toString();
      } catch (_) {}
    }
  }

  Future<void> openUpdate() async {
    final raw = apkUrl ?? downloadPage;
    if (raw.isEmpty) return;
    final uri = Uri.parse(raw);
    await launchUrl(uri, mode: LaunchMode.externalApplication);
  }

  Future<void> signIn({
    required String username,
    required String password,
    String? name,
    bool register = false,
  }) async {
    lastError = '';
    notifyListeners();
    try {
      if (register) {
        await api.register(username: username, password: password, displayName: name);
      } else {
        await api.login(username: username, password: password);
      }
      displayName = api.displayName ?? username;
      pairCode = api.pairCode ?? '';
      signedIn = true;
      await api.bind(deviceId: 'watch-esp32-1', code: pairCode, name: 'BondWatch');
      await api.bind(
        deviceId: kIsWeb ? 'pc-companion' : 'phone-companion',
        code: pairCode,
        name: kIsWeb ? '电脑' : 'Android',
      );
      bound = true;
      sessionId = await api.openSession(holder: kIsWeb ? 'pc' : 'phone');
      await refreshCloud();
      await _pollFace();
      emotion = liveWatch ? emotion : 'idle';
      subtitle = liveWatch ? subtitle : '已绑定 $pairCode。点脸说话，手表同步';
    } catch (error) {
      lastError = error.toString();
      signedIn = false;
    }
    notifyListeners();
  }

  Future<void> joinWithCode(String code) async {
    lastError = '';
    notifyListeners();
    try {
      api.baseUrl = api.baseUrl;
      final data = await api.joinPair(
        code: code,
        holder: kIsWeb ? 'pc' : 'phone',
        name: kIsWeb ? '电脑' : 'Android',
      );
      displayName = api.displayName ?? 'companion';
      pairCode = api.pairCode ?? code;
      signedIn = true;
      bound = true;
      sessionId = data['session_id']?.toString() ?? await api.openSession(holder: kIsWeb ? 'pc' : 'phone');
      try {
        await api.bind(deviceId: 'watch-esp32-1', code: pairCode, name: 'BondWatch');
      } catch (_) {}
      await refreshCloud();
      await _pollFace();
      if (!liveWatch) {
        emotion = 'idle';
        subtitle = '已绑定 $pairCode。等手表说话';
      }
    } catch (error) {
      lastError = error.toString();
      signedIn = false;
    }
    notifyListeners();
  }

  Future<void> _pollFace() async {
    if (api.token == null) return;
    try {
      final data = await api.watchFace();
      final seq = data['seq'] as int? ?? 0;
      final source = data['source']?.toString() ?? '';
      final text = data['text']?.toString() ?? '';
      final emo = data['emotion']?.toString() ?? 'idle';
      if (seq > 0 && seq != faceSeq && text.isNotEmpty) {
        faceSeq = seq;
        faceSource = source;
        liveWatch = source == 'watch' || source == 'phone' || source == 'pc';
        emotion = emo;
        subtitle = text;
        screenOn = true;
        notifyListeners();
      }
      online = true;
    } catch (_) {}
  }

  Future<void> _pollAudio() async {
    if (api.token == null) return;
    try {
      final data = await api.watchAudio();
      final seq = data['seq'] as int? ?? 0;
      final url = data['url']?.toString() ?? '';
      if (seq > 0 && seq != audioSeq && url.isNotEmpty) {
        audioSeq = seq;
        audioUrl = api.watchAudioUrl(url);
        audioSeconds = (data['seconds'] as num?)?.toDouble() ?? 0;
        notifyListeners();
      }
    } catch (_) {}
  }

  Future<void> openWatchAudio() async {
    if (audioUrl.isEmpty) return;
    await launchUrl(Uri.parse(audioUrl), mode: LaunchMode.externalApplication);
  }

  Future<void> refreshCloud() async {
    if (api.token == null) return;
    try {
      final mem = await api.memory();
      persona = mem['persona']?.toString() ?? '';
      notes = mem['notes']?.toString() ?? '';
      final set = await api.settings();
      volume = set['volume'] as int? ?? 70;
      dnd = set['dnd'] == true;
      allowProactive = set['allow_proactive'] == true;
      speakingMuted = dnd || volume == 0;
      final alarmRes = await api.alarms();
      alarms = [
        for (final item in alarmRes['items'] as List<dynamic>? ?? [])
          LocalAlarm(
            id: item['id'].toString(),
            fireAt: DateTime.parse(item['fire_at'].toString()).toLocal(),
            label: item['label']?.toString() ?? '闹钟',
          ),
      ];
      final items = await api.history();
      history = [
        for (final item in items)
          {
            'role': item['role']?.toString() ?? '',
            'text': item['text']?.toString() ?? '',
            'emotion': item['emotion']?.toString() ?? '',
          },
      ];
      online = true;
      lastError = '';
    } catch (error) {
      online = false;
      lastError = error.toString();
    }
    notifyListeners();
  }

  Future<void> shootAndTalk() async {
    lastError = '';
    notifyListeners();
    try {
      final mobile = defaultTargetPlatform == TargetPlatform.android ||
          defaultTargetPlatform == TargetPlatform.iOS;
      final shot = await ImagePicker().pickImage(
        source: mobile ? ImageSource.camera : ImageSource.gallery,
        maxWidth: 320,
        maxHeight: 320,
        imageQuality: 35,
      );
      if (shot == null) {
        return;
      }
      await tapTalk('看看这是什么', imageBase64: base64Encode(await shot.readAsBytes()));
    } catch (error) {
      lastError = '摄像头打不开：$error';
      notifyListeners();
    }
  }

  Future<void> tapTalk(String text, {String? imageBase64}) async {
    if (!screenOn) {
      screenOn = true;
    }
    emotion = 'listen';
    subtitle = imageBase64 == null ? '聆听中…' : '看着照片…';
    notifyListeners();
    await Future<void>.delayed(const Duration(milliseconds: 700));
    emotion = 'think';
    subtitle = '思考中…';
    notifyListeners();
    if (!online || sessionId.isEmpty) {
      await Future<void>.delayed(const Duration(milliseconds: 600));
      emotion = 'offline';
      subtitle = '没网，这轮先记下。来网再补。';
      notifyListeners();
      return;
    }
    try {
      final reply = await api.turn(sessionId, text, imageBase64: imageBase64);
      emotion = speakingMuted ? 'silent' : (reply['emotion']?.toString() ?? 'speak');
      subtitle = reply['subtitle']?.toString() ?? reply['text']?.toString() ?? '';
      faceSource = kIsWeb ? 'pc' : 'phone';
      try {
        final face = await api.publishFace(
          emotion: emotion,
          text: subtitle,
          source: faceSource,
        );
        faceSeq = face['seq'] as int? ?? faceSeq;
      } catch (_) {}
      await refreshCloud();
    } catch (error) {
      emotion = 'offline';
      subtitle = '云端失败：$error';
    }
    notifyListeners();
    await Future<void>.delayed(const Duration(seconds: 2));
    if (!liveWatch && (emotion == 'speak' || emotion == 'silent' || emotion == 'quiet')) {
      emotion = dnd ? 'silent' : 'idle';
      subtitle = dnd ? '勿扰。字幕仍在。' : '点屏幕或按对讲';
      notifyListeners();
    }
  }

  void interrupt() {
    emotion = dnd ? 'silent' : 'idle';
    subtitle = '已打断';
    notifyListeners();
  }

  void shortPower() {
    screenOn = !screenOn;
    if (!screenOn) {
      emotion = 'idle';
      subtitle = '';
    } else if (dnd) {
      emotion = 'silent';
      subtitle = '勿扰。字幕仍在。';
    }
    notifyListeners();
  }

  Future<void> toggleDnd() async {
    dnd = !dnd;
    speakingMuted = dnd || volume == 0;
    emotion = dnd ? 'silent' : 'idle';
    subtitle = dnd ? '勿扰：喇叭为零，字幕强制开' : '勿扰已关';
    notifyListeners();
    if (online) {
      try {
        await api.saveSettings({'dnd': dnd});
      } catch (_) {}
    }
  }

  Future<void> setVolume(int value) async {
    volume = value;
    speakingMuted = dnd || volume == 0;
    if (volume == 0) {
      emotion = 'silent';
      subtitle = subtitle.isEmpty ? '静音，字幕打开' : subtitle;
    }
    notifyListeners();
    if (online) {
      try {
        await api.saveSettings({'volume': volume});
      } catch (_) {}
    }
  }

  Future<void> setProactive(bool value) async {
    allowProactive = value;
    notifyListeners();
    if (online) {
      try {
        await api.saveSettings({'allow_proactive': value});
      } catch (_) {}
    }
  }

  Future<void> saveMemory() async {
    if (!online) return;
    await api.saveMemory(persona, notes);
    await refreshCloud();
  }

  Future<void> addOneMinuteAlarm() async {
    final local = LocalAlarm(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      fireAt: DateTime.now().add(const Duration(minutes: 1)),
      label: '本地闹钟',
    );
    alarms = [...alarms, local];
    notifyListeners();
    if (online) {
      try {
        await api.addAlarm(minutes: 1);
        await refreshCloud();
      } catch (_) {}
    }
  }

  void _tickAlarms() {
    final now = DateTime.now();
    LocalAlarm? due;
    for (final alarm in alarms) {
      if (!alarm.fireAt.isAfter(now)) {
        due = alarm;
        break;
      }
    }
    if (due == null) return;
    alarms = alarms.where((item) => item.id != due!.id).toList();
    screenOn = true;
    emotion = 'alarm';
    subtitle = '${due.label} · 本地铃，不请求模型';
    notifyListeners();
  }

  @override
  void dispose() {
    _ticker?.cancel();
    _facePoll?.cancel();
    super.dispose();
  }
}
