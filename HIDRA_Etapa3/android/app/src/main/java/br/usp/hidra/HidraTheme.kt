package br.usp.hidra

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.size
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.unit.dp

private val LightColors = lightColorScheme(
    primary=Color(0xFF006B68), onPrimary=Color.White,
    primaryContainer=Color(0xFFD5F3EC), onPrimaryContainer=Color(0xFF003C3A),
    secondary=Color(0xFF41617B), secondaryContainer=Color(0xFFE1EDF5),
    background=Color(0xFFF3F7F8), onBackground=Color(0xFF182D34),
    surface=Color.White, onSurface=Color(0xFF182D34),
    surfaceVariant=Color(0xFFE8EFF1), onSurfaceVariant=Color(0xFF465C64),
    outline=Color(0xFF71868C), error=Color(0xFF9E3036),
    errorContainer=Color(0xFFFFE9E7), onErrorContainer=Color(0xFF741C24)
)
private val DarkColors = darkColorScheme(
    primary=Color(0xFF8ED9CE), onPrimary=Color(0xFF003735),
    primaryContainer=Color(0xFF164D49), onPrimaryContainer=Color(0xFFC3F4EB),
    background=Color(0xFF101D22), onBackground=Color(0xFFDFEBEF),
    surface=Color(0xFF1B2B32), onSurface=Color(0xFFDFEBEF),
    surfaceVariant=Color(0xFF293E46), onSurfaceVariant=Color(0xFFB7CBD2),
    error=Color(0xFFFFB3B3), errorContainer=Color(0xFF57282B), onErrorContainer=Color(0xFFFFDAD8)
)
@Composable fun HidraTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme=if(isSystemInDarkTheme()) DarkColors else LightColors, content=content)
}
@Composable fun WaterMark() {
    val color=MaterialTheme.colorScheme.primary
    Canvas(Modifier.size(34.dp)) {
        val w=size.width; val h=size.height
        val drop=Path().apply {
            moveTo(w*.5f,h*.04f)
            cubicTo(w*.40f,h*.26f,w*.12f,h*.48f,w*.12f,h*.66f)
            cubicTo(w*.12f,h*1.04f,w*.88f,h*1.04f,w*.88f,h*.66f)
            cubicTo(w*.88f,h*.48f,w*.60f,h*.26f,w*.5f,h*.04f)
            close()
        }
        drawPath(drop,color)
    }
}
