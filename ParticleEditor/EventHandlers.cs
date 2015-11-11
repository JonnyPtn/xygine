﻿/*********************************************************************
Matt Marchant 2015
http://trederia.blogspot.com

xygine Particle Editor - Zlib license.

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.
*********************************************************************/

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using SFML.Window;
using SFML.Graphics;
using System.Drawing;

namespace ParticleEditor
{
    public partial class MainWindow : Form
    {
        private void buttonStart_Click(object sender, EventArgs e)
        {
            if (m_particleSystem.started)
            {
                buttonStart.Text = "Start";
                m_particleSystem.stop();
            }
            else
            {
                buttonStart.Text = "Stop";
                m_particleSystem.start((byte)numericUpDownReleaseCount.Value, (float)numericUpDownStartDelay.Value, (float)numericUpDownDuration.Value);
            }
        }

        private void buttonSpawnPosAdd_Click(object sender, EventArgs e)
        {
            listBoxSpawnPoints.Items.Add(numericUpDownSpawnPosX.Value.ToString() + ", " + numericUpDownSpawnPosY.Value.ToString());
            var list = parseList(listBoxSpawnPoints.Items);
            m_particleSystem.randomInitialPositions = (list.Count > 0) ? list : null;
        }

        private void buttonSpawnVelAdd_Click(object sender, EventArgs e)
        {
            listBoxSpawnVelocities.Items.Add(numericUpDownSpawnVelX.Value.ToString() + ", " + numericUpDownSpawnVelY.Value.ToString());
            var list = parseList(listBoxSpawnVelocities.Items);
            m_particleSystem.randomInitialVelocities = (list.Count > 0) ? list : null;
        }

        private char[] delim = { ',' };
        private List<Vector2f> parseList(ListBox.ObjectCollection items)
        {
            List<Vector2f> retVal = new List<Vector2f>();
            foreach (string s in items)
            {
                var pair = s.Split(delim);
                Vector2f v = new Vector2f(float.Parse(pair[0]), float.Parse(pair[1]));
                retVal.Add(v);
            }
            return retVal;
        }

        private void itemDelete_click(object sender, EventArgs e)
        {
            MenuItem item = sender as MenuItem;
            var menu = item.GetContextMenu();
            ListBox box = menu.SourceControl as ListBox;

            if (box.SelectedIndex > -1)
            {
                box.Items.RemoveAt(box.SelectedIndex);
            }

            var list = parseList(box.Items);
            if (box == listBoxSpawnPoints)
            {
                m_particleSystem.randomInitialPositions = (list.Count > 0) ? list : null;
            }
            else if (box == listBoxSpawnVelocities)
            {
                m_particleSystem.randomInitialVelocities = (list.Count > 0) ? list : null;
            }
        }

        private void itemsClear_click(object sender, EventArgs e)
        {
            MenuItem item = sender as MenuItem;
            var menu = item.GetContextMenu();
            ListBox box = menu.SourceControl as ListBox;

            box.Items.Clear();

            var list = parseList(box.Items);
            if (box == listBoxSpawnPoints)
            {
                m_particleSystem.randomInitialPositions = (list.Count > 0) ? list : null;
            }
            else if (box == listBoxSpawnVelocities)
            {
                m_particleSystem.randomInitialVelocities = (list.Count > 0) ? list : null;
            }
        }

        private Texture m_texture;
        private void buttonTextureBrowse_Click(object sender, EventArgs e)
        {
            OpenFileDialog od = new OpenFileDialog();
            od.Filter = "JPEG files|*.jpg|Portable Network Graphic|*.png|Bitmap files|*.bmp";
            if (od.ShowDialog() == DialogResult.OK)
            {
                m_texture = new Texture(od.FileName);
                m_particleSystem.texture = m_texture;
                textBoxTexturePath.Text = Path.GetFileName(od.FileName);
                panelTexPreview.BackgroundImage = new System.Drawing.Bitmap(od.FileName);
            }
        }

        private void buttonTextureFit_Click(object sender, EventArgs e)
        {
            if (m_particleSystem.texture != null)
            {
                var texSize = m_particleSystem.texture.Size;
                numericUpDownSizeX.Value = Math.Min(texSize.X, numericUpDownSizeX.Maximum);
                numericUpDownSizeY.Value = Math.Min(texSize.Y, numericUpDownSizeY.Maximum);
            }
        }

        private void sizeChanged(object sender, EventArgs e)
        {
            Vector2f size = new Vector2f((float)numericUpDownSizeX.Value, (float)numericUpDownSizeY.Value);
            m_particleSystem.particleSize = size;
        }

        private void buttonClearTexture_Click(object sender, EventArgs e)
        {
            m_texture = null;
            m_particleSystem.texture = null;
            textBoxTexturePath.Text = string.Empty;
            panelTexPreview.BackgroundImage = null;
        }

        private void comboBoxBlendMode_SelectedIndexChanged(object sender, EventArgs e)
        {
            m_particleSystem.blendMode = (BlendMode)comboBoxBlendMode.SelectedItem;
        }

        private void velocityChanged(object sender, EventArgs e)
        {
            Vector2f initVel = new Vector2f((float)numericUpDownInitVelX.Value, (float)numericUpDownInitVelY.Value);
            m_particleSystem.initialVelocity = initVel;
        }

        private void colour_Click(object sender, EventArgs e)
        {
            ColorDialog cd = new ColorDialog();
            cd.Color = panelColour.BackColor;
            cd.AnyColor = true;
            cd.FullOpen = true;
            if (cd.ShowDialog() == DialogResult.OK)
            {
                panelColour.BackColor = cd.Color;
            }
        }

        private void PanelColour_BackColorChanged(object sender, EventArgs e)
        {
            SFML.Graphics.Color colour = new SFML.Graphics.Color();
            colour.R = panelColour.BackColor.R;
            colour.G = panelColour.BackColor.G;
            colour.B = panelColour.BackColor.B;
            m_particleSystem.colour = colour;
        }

        private void EnableMovementToolStripMenuItem_CheckedChanged(object sender, EventArgs e)
        {
            m_particleSystem.position = new Vector2f();
        }

        private void backgroundColourToolStripMenuItem_Click(object sender, EventArgs e)
        {
            ColorDialog cd = new ColorDialog();
            cd.AnyColor = true;
            cd.FullOpen = true;
            if (cd.ShowDialog() == DialogResult.OK)
            {
                SFML.Graphics.Color colour = new SFML.Graphics.Color();
                colour.R = cd.Color.R;
                colour.G = cd.Color.G;
                colour.B = cd.Color.B;
                m_sfmlControl.BackgroundColour = colour;
            }
        }

        private void NumericUpDownEmitRate_ValueChanged(object sender, EventArgs e)
        {
            m_particleSystem.emitRate = (float)numericUpDownEmitRate.Value;
        }

        private void newToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("Save current file?", "Confirm", MessageBoxButtons.YesNo) == DialogResult.Yes)
            {
                saveAsToolStripMenuItem_Click(sender, e);
            }

            listBoxAffectors.Items.Clear();
            listBoxSpawnPoints.Items.Clear();
            listBoxSpawnVelocities.Items.Clear();

            numericUpDownReleaseCount.Value = 1;
            numericUpDownStartDelay.Value = 0;
            numericUpDownDuration.Value = -1;
            numericUpDownEmitRate.Value = 30;

            panelTexPreview.BackgroundImage = null;
            textBoxTexturePath.Text = string.Empty;

            comboBoxBlendMode.SelectedItem = BlendMode.Add;

            numericUpDownInitVelX.Value = 0;
            numericUpDownInitVelY.Value = 0;
            numericUpDownSizeX.Value = 2;
            numericUpDownSizeY.Value = 2;

            panelColour.BackColor = System.Drawing.Color.White;
            numericUpDownLifetime.Value = 1;

            numericUpDownSpawnPosX.Value = 0;
            numericUpDownSpawnPosY.Value = 0;
            numericUpDownSpawnVelX.Value = 0;
            numericUpDownSpawnVelY.Value = 0;

            m_texture = null;
            //m_particleSystem = new ParticleSystem();
            m_particleSystem.randomInitialPositions = null;
            m_particleSystem.randomInitialVelocities = null;
            m_filePath = string.Empty;
        }

        private void saveToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (m_filePath == string.Empty)
            {
                saveAsToolStripMenuItem_Click(sender, e);
            }
            else
            {
                saveFile();
            }

        }

        private void saveAsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SaveFileDialog sd = new SaveFileDialog();
            sd.Filter = "xygine particle file|*.xyp";
            if(sd.ShowDialog() == DialogResult.OK)
            {
                m_filePath = sd.FileName;
                saveFile();
            }
        }

        private void openToolStripMenuItem_Click(object sender, EventArgs e)
        {
            //if(m_filePath != string.Empty)
            {
                if(MessageBox.Show("Save current file?", "Confirm", MessageBoxButtons.YesNo) == DialogResult.Yes)
                {
                    saveAsToolStripMenuItem_Click(sender, e);
                }
            }

            OpenFileDialog od = new OpenFileDialog();
            od.Filter = "xygine particle file|*.xyp";
            if(od.ShowDialog() == DialogResult.OK)
            {
                m_filePath = od.FileName;
                loadFile();
            }
        }

        private void NumericUpDownLifetime_ValueChanged(object sender, EventArgs e)
        {
            m_particleSystem.particleLifetime = (float)numericUpDownLifetime.Value;
        }

        private void buttonAddAffector_Click(object sender, EventArgs e)
        {
            var selected = (AffectorType)comboBoxAffectors.SelectedItem;
            AffectorWindow aw = new AffectorWindow(selected);
            if(aw.ShowDialog() == DialogResult.OK)
            {
                var affector = aw.Affector;

                m_particleSystem.addAffector(affector);
                listBoxAffectors.Items.Add(affector.type().ToString());
            }
        }

        private Random random = new Random();
        private void buttonRandPos_Click(object sender, EventArgs e)
        {
            numericUpDownSpawnPosX.Value = random.Next(-40, 40);
            numericUpDownSpawnPosY.Value = random.Next(-40, 40);
        }

        private void buttonRandVel_Click(object sender, EventArgs e)
        {
            numericUpDownSpawnVelX.Value = random.Next(-100, 100);
            numericUpDownSpawnVelY.Value = random.Next(-100, 100);
        }

        private void buttonRandomForce_Click(object sender, EventArgs e)
        {
            numericUpDownForceX.Value = random.Next(-100, 100);
            numericUpDownForceY.Value = random.Next(-100, 100);
        }

        private void numericUpDownForce_ValueChanged(object sender, EventArgs e)
        {
            if(listBoxAffectors.SelectedIndex > -1)
            {
                Vector2f force = new Vector2f((float)numericUpDownForceX.Value, (float)numericUpDownForceY.Value);
                ((ForceAffector)m_particleSystem.Affectors[listBoxAffectors.SelectedIndex]).Force = force;
            }
        }

        private void listBoxAffectors_SelectedIndexChanged(object sender, EventArgs e)
        {
            panelForceAffector.Location = new Point(10000, 0); //move it out of the visible area

            if (listBoxAffectors.SelectedIndex > -1)
            {
                int idx = listBoxAffectors.SelectedIndex;
                string name = listBoxAffectors.Items[idx].ToString();
                Point location = new Point(160, 23);
                switch (name)
                {
                    case "Force":
                        var force = ((ForceAffector)m_particleSystem.Affectors[idx]).Force;
                        numericUpDownForceX.Value = (Decimal)force.X;
                        numericUpDownForceY.Value = (Decimal)force.Y;
                        panelForceAffector.Location = location;
                        break;
                    case "Colour":

                        break;
                    case "Scale":

                        break;
                    case "Rotation":

                        break;
                }
            }
        }
    }
}
