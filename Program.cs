using System.Net.Sockets;
using System.Text;
using UdpForwarder;

if (args.Length is < 3)
{
    Console.WriteLine("At least 3 values are necessary. Mode (1 or 2) Port A, Port B");
    return;
}

var mode = int.Parse(args[0]);
var portA = int.Parse(args[1]);
var portB = int.Parse(args[2]);

var cts = new CancellationTokenSource();
var ct = cts.Token;

Console.CancelKeyPress += (_, _) => cts.Cancel();

if (mode == 1)
{
    // These wrapper are used to comunicate between the 2 invocations of ForwardMessagesAsync
    var desEndpointA = new IPEndpointWrapper();
    var desEndpointB = new IPEndpointWrapper();

    using var clientA = new UdpClient(portA);
    using var clientB = new UdpClient(portB);

    Console.WriteLine($"Listening on port {portA} and {portB}");
    Console.WriteLine($"Starting forwarding for A and B.");

    var taskA = ForwardMessagesAsync(clientA, clientB, "A", "B", desEndpointA, desEndpointB, ct);
    var taskB = ForwardMessagesAsync(clientB, clientA, "B", "A", desEndpointB, desEndpointA, ct);

    await Task.WhenAll([taskA, taskB]);
    return;
}

if (mode == 2)
{
    if (args.Length is not 5)
    {
        Console.WriteLine("In mode 2 (recv) 5 values are necessary. Mode, Port A, Port B, hostname A, hostname B");
        return;
    }

    var hostnameA = args[3];
    var hostnameB = args[4];

    using var clientA = new UdpClient(hostnameA, portA);
    using var clientB = new UdpClient(hostnameB, portB);

    Console.WriteLine("Sending initial message to A");

    await clientA.SendAsync(Encoding.UTF8.GetBytes("X"));

    Console.WriteLine($"Starting forwarding for A and B.");

    var taskA = ForwardMessagesAsync(clientA, clientB, "A", "B", null, null, ct);
    var taskB = ForwardMessagesAsync(clientB, clientA, "B", "A", null, null, ct);

    await Task.WhenAll([taskA, taskB]);
    return;
}

Console.WriteLine("Unknown mode.");

static async Task ForwardMessagesAsync(UdpClient source, UdpClient destination, string sourceName, string destName,
    IPEndpointWrapper? ownWrapper, IPEndpointWrapper? endPointWrapper, CancellationToken ct)
{
    if (ownWrapper is not null && endPointWrapper is not null)
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                var result = await source.ReceiveAsync(ct);

                ownWrapper.EndPoint = result.RemoteEndPoint;

                if (endPointWrapper.EndPoint is null)
                {
                    Console.WriteLine("Ignoring the message. Remote endpoint for destination not available.");
                    continue;
                }

                await destination.SendAsync(result.Buffer, endPointWrapper.EndPoint, ct);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in forwarding message from {sourceName} to {destName}: {ex.Message}");
            }
        }
    }
    else
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                var result = await source.ReceiveAsync(ct);

                await destination.SendAsync(result.Buffer, ct);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in forwarding message from {sourceName} to {destName}: {ex.Message}");
            }
        }
    }
}