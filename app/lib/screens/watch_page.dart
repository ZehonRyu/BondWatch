import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../models/emotion.dart';
import '../state/bond_state.dart';

class WatchPage extends StatefulWidget {
  const WatchPage({super.key, required this.state});

  final BondState state;

  @override
  State<WatchPage> createState() => _WatchPageState();
}

class _WatchPageState extends State<WatchPage> {
  final _text = TextEditingController();
  DateTime _pwrTapAt = DateTime.fromMillisecondsSinceEpoch(0);

  @override
  void dispose() {
    _text.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = widget.state;
    final style = EmotionStyle.of(state.emotion);
    return ListenableBuilder(
      listenable: state,
      builder: (context, _) {
        return Container(
          color: const Color(0xFF07070A),
          child: SafeArea(
            child: Column(
              children: [
                const SizedBox(height: 12),
                Text(
                  state.bound ? '假手表 · 已绑定' : '假手表 · 未绑定',
                  style: const TextStyle(color: Colors.white54, fontSize: 13),
                ),
                const SizedBox(height: 16),
                Expanded(
                  child: Center(
                    child: GestureDetector(
                      onTap: () {
                        if (!state.screenOn) {
                          state.shortPower();
                          return;
                        }
                        if (state.emotion == 'speak') {
                          state.interrupt();
                          return;
                        }
                        final text = _text.text.trim().isEmpty ? '你好' : _text.text.trim();
                        state.tapTalk(text);
                      },
                      child: _face(state, style),
                    ),
                  ),
                ),
                const Padding(
                  padding: EdgeInsets.only(bottom: 8),
                  child: Text(
                    '点脸说话 · 金色键拍照给小精灵看',
                    style: TextStyle(color: Colors.white38, fontSize: 12),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.fromLTRB(20, 8, 20, 8),
                  child: TextField(
                    controller: _text,
                    enabled: state.screenOn,
                    style: const TextStyle(color: Colors.white),
                    decoration: InputDecoration(
                      hintText: '用文字代替麦克风',
                      hintStyle: const TextStyle(color: Colors.white38),
                      filled: true,
                      fillColor: const Color(0xFF1A1A22),
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(16),
                        borderSide: BorderSide.none,
                      ),
                    ),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.fromLTRB(28, 4, 28, 24),
                  child: Row(
                    children: [
                      _key(
                        color: const Color(0xFF2ECC71),
                        label: 'PWR',
                        onTap: () {
                          final now = DateTime.now();
                          if (now.difference(_pwrTapAt).inMilliseconds < 350) {
                            state.toggleDnd();
                          } else {
                            state.shortPower();
                          }
                          _pwrTapAt = now;
                        },
                        onLongPress: state.shortPower,
                      ),
                      const Spacer(),
                      _key(
                        color: const Color(0xFFE7C27D),
                        label: '看',
                        onTap: state.shootAndTalk,
                      ),
                      const Spacer(),
                      _key(
                        color: const Color(0xFF5DADE2),
                        label: 'PTT',
                        onTap: () {
                          if (state.emotion == 'speak') {
                            state.interrupt();
                            return;
                          }
                          final text = _text.text.trim().isEmpty ? '你好' : _text.text.trim();
                          state.tapTalk(text);
                        },
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Widget _face(BondState state, EmotionStyle style) {
    if (!state.screenOn) {
      return Container(
        width: 240,
        height: 280,
        decoration: BoxDecoration(
          color: Colors.black,
          borderRadius: BorderRadius.circular(32),
          border: Border.all(color: Colors.white10),
        ),
        child: const Center(
          child: Text('息屏', style: TextStyle(color: Colors.white24)),
        ),
      );
    }
    return Container(
      width: 240,
      height: 280,
      decoration: BoxDecoration(
        color: Color(style.bg),
        borderRadius: BorderRadius.circular(32),
        boxShadow: const [BoxShadow(color: Colors.black54, blurRadius: 24)],
      ),
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 10),
      child: Column(
        children: [
          Text(
            TimeOfDay.now().format(context),
            style: const TextStyle(color: Colors.white70, fontSize: 14, letterSpacing: 2),
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: 168,
            height: 140,
            child: ElfFace(emotion: state.emotion, skin: Color(style.face)),
          ),
          Text(
            style.label,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 18,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 4),
          Expanded(
            child: Text(
              state.subtitle,
              textAlign: TextAlign.center,
              maxLines: 3,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(color: Colors.white70, fontSize: 12, height: 1.3),
            ),
          ),
          Text(
            state.speakingMuted ? '喇叭关 · 字幕开' : '音量 ${state.volume}',
            style: const TextStyle(color: Colors.white38, fontSize: 11),
          ),
        ],
      ),
    );
  }

  Widget _key({
    required Color color,
    required String label,
    required VoidCallback onTap,
    VoidCallback? onLongPress,
  }) {
    return GestureDetector(
      onTap: onTap,
      onLongPress: onLongPress,
      child: Column(
        children: [
          Container(
            width: 72,
            height: 72,
            decoration: BoxDecoration(
              color: color,
              shape: BoxShape.circle,
              boxShadow: [BoxShadow(color: color.withValues(alpha: 0.4), blurRadius: 16)],
            ),
          ),
          const SizedBox(height: 8),
          Text(label, style: const TextStyle(color: Colors.white70)),
        ],
      ),
    );
  }
}

class ElfFace extends StatefulWidget {
  const ElfFace({super.key, required this.emotion, required this.skin});

  final String emotion;
  final Color skin;

  @override
  State<ElfFace> createState() => _ElfFaceState();
}

class _ElfFaceState extends State<ElfFace> with SingleTickerProviderStateMixin {
  late final AnimationController _tick;

  @override
  void initState() {
    super.initState();
    _tick = AnimationController(vsync: this, duration: const Duration(seconds: 2))..repeat();
  }

  @override
  void dispose() {
    _tick.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _tick,
      builder: (context, _) {
        return CustomPaint(
          painter: _ElfPainter(widget.emotion, widget.skin, DateTime.now().millisecondsSinceEpoch),
        );
      },
    );
  }
}

class _ElfPainter extends CustomPainter {
  _ElfPainter(this.emotion, this.skin, this.nowMs);
  final String emotion;
  final Color skin;
  final int nowMs;

  @override
  void paint(Canvas canvas, Size size) {
    final t = nowMs / 1000.0;
    final breath = 1.0 + 0.03 * math.sin(t * 2.2);
    var tilt = 0.0;
    if (emotion == 'think') {
      tilt = -0.08 + 0.05 * math.sin(t * 2);
    } else if (emotion == 'alarm') {
      tilt = 0.06 * math.sin(t * 28);
    } else if (emotion == 'listen') {
      tilt = 0.02 * math.sin(t * 6);
    }
    canvas.translate(size.width / 2, size.height * 0.72);
    canvas.rotate(tilt);
    canvas.scale(breath);
    canvas.translate(-size.width / 2, -size.height * 0.72);

    final skinPaint = Paint()..color = skin;
    final ink = Paint()
      ..color = const Color(0xFF1A1408)
      ..strokeWidth = 3
      ..strokeCap = StrokeCap.round;
    final cx = size.width / 2;
    final earWiggle = emotion == 'listen' ? 6 * math.sin(t * 8) : 0.0;
    final head = Offset(cx, size.height * 0.62);
    final ear = Path()
      ..moveTo(cx - 58, head.dy - 8)
      ..lineTo(cx - 36 - earWiggle, head.dy - 58)
      ..lineTo(cx - 16, head.dy - 18)
      ..close();
    final earR = Path()
      ..moveTo(cx + 58, head.dy - 8)
      ..lineTo(cx + 36 + earWiggle, head.dy - 58)
      ..lineTo(cx + 16, head.dy - 18)
      ..close();
    canvas.drawPath(ear, skinPaint);
    canvas.drawPath(earR, skinPaint);
    canvas.drawCircle(head, 52, skinPaint);
    final spark = 0.55 + 0.45 * (0.5 + 0.5 * math.sin(t * 4));
    canvas.drawCircle(
      Offset(cx, 10),
      2.6 + 1.4 * spark,
      Paint()..color = Color.fromRGBO(255, 246, 194, spark),
    );
    final blink = (t % 3.2) > 3.05;
    if (blink) {
      canvas.drawLine(Offset(cx - 24, head.dy - 10), Offset(cx - 12, head.dy - 10), ink);
      canvas.drawLine(Offset(cx + 12, head.dy - 10), Offset(cx + 24, head.dy - 10), ink);
    } else {
      canvas.drawCircle(Offset(cx - 18, head.dy - 10), 5, ink..style = PaintingStyle.fill);
      canvas.drawCircle(Offset(cx + 18, head.dy - 10), 5, ink);
    }
    canvas.drawOval(
      Rect.fromCenter(center: Offset(cx - 28, head.dy + 4), width: 14, height: 8),
      Paint()..color = const Color(0x55FF7896),
    );
    canvas.drawOval(
      Rect.fromCenter(center: Offset(cx + 28, head.dy + 4), width: 14, height: 8),
      Paint()..color = const Color(0x55FF7896),
    );
    ink.style = PaintingStyle.stroke;
    final mouth = Offset(cx, head.dy + 18);
    if (emotion == 'listen' || emotion == 'speak' || emotion == 'alarm') {
      final open = 0.35 + 0.65 * math.sin(t * 10).abs();
      canvas.drawOval(Rect.fromCenter(center: mouth, width: 18 + 6 * open, height: 6 + 12 * open), ink);
    } else if (emotion == 'think') {
      canvas.drawLine(Offset(cx - 12, mouth.dy), Offset(cx + 12, mouth.dy - 5 + 2 * math.sin(t * 3)), ink);
    } else {
      canvas.drawLine(Offset(cx - 12, mouth.dy), Offset(cx + 12, mouth.dy), ink);
    }
  }

  @override
  bool shouldRepaint(covariant _ElfPainter oldDelegate) =>
      oldDelegate.emotion != emotion || oldDelegate.skin != skin || oldDelegate.nowMs != nowMs;
}
