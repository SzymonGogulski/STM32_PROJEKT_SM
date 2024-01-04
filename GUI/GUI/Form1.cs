using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Windows.Forms.DataVisualization.Charting;

namespace GUI
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
            serialPort1.DataReceived += new SerialDataReceivedEventHandler(DataReceivedHandler);
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            string[] ports = SerialPort.GetPortNames();
            cBoxCOMPORT.Items.AddRange(ports);

            btnConnect.Enabled = true;
            btnDisconnect.Enabled = false;
        }

        private void btnConnect_Click(object sender, EventArgs e)
        {
            try {
                serialPort1.PortName = cBoxCOMPORT.Text;
                serialPort1.BaudRate = Convert.ToInt32(cBoxBaudRate.Text);
                serialPort1.DataBits = Convert.ToInt32(cBoxDataBits.Text);
                serialPort1.StopBits = (StopBits)Enum.Parse(typeof(StopBits), cBoxStopBits.Text);
                serialPort1.Parity = (Parity)Enum.Parse(typeof(Parity), cBoxParityBits.Text);

                serialPort1.Open();
                progressBar1.Value = 100;
                btnConnect.Enabled = false;
                btnDisconnect.Enabled = true;

            } catch (Exception err){
                MessageBox.Show(err.Message, "Error during connect!", MessageBoxButtons.OK, MessageBoxIcon.Error);
                btnConnect.Enabled = true;
                btnDisconnect.Enabled = false;
            }
        }

        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
                progressBar1.Value = 0;
                btnConnect.Enabled = true;
                btnDisconnect.Enabled = false;
            }
        }

        private void DataReceivedHandler(object sender, SerialDataReceivedEventArgs e)
        {
            SerialPort sp = (SerialPort)sender;
            string dataIn = sp.ReadLine().TrimEnd(';');
            double temperature;

            // Wyświetlanie aktualnej temperatury
            lblCurrentTemperature.Text = dataIn;

            // Aktualizacja wykresu
            if (double.TryParse(dataIn, out temperature))
            {
                UpdateChart(temperature);
            }
        }

        private void UpdateChart(double temperature)
        {
            this.Invoke((MethodInvoker)delegate
            {
                // Temperatura odczytana
                chart1.Series[0].Points.AddY(temperature);
                // Temperatura zadana
                chart1.Series[1].Points.AddY(55.0);

            });
        }

        private void btnSetTemperature_Click(object sender, EventArgs e)
        {
        }

        private void btnSetPidSettings_Click(object sender, EventArgs e)
        {
        }
    }
}
